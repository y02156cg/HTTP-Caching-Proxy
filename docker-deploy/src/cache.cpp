#include "cache.hpp"

/**
 * Using LRU (Least Recently Used) replacement policy to update the given URL in the cache
 * @note This function removes the given `url` from the LRU list and moves it to the front, 
 *       indicating that it was recently accessed.
 *
 * @param url The URL whose position in the LRU list needs to be updated.
 */
void Cache::updateLRU(const string& url){
    lru_list.remove(url);
    lru_list.push_front(url);
}

/**
 * Determines if a cached response has expired.
 * @note This function parses the `Expire` time of the response and compares it with the current time.
 *       If no expiration time is found, the response is considered expired.
 *
 * @param response Pointer to the `Response` object to check for expiration.
 * @return `true` if the response has expired, `false` otherwise.
 */
bool Cache::isExpired(const Response* response) const{
    if(response->getExpireTime().empty()){
        return true;
    }

    tm tm = {};
    istringstream ss(response->getExpireTime());
    ss >> get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");

    auto time_t = mktime(&tm);
    auto expireTime_chrono = chrono::system_clock::from_time_t(time_t);
    auto now = chrono::system_clock::now();
    return now > expireTime_chrono;
}

/**
 * Removes the least recently used (LRU) cached entries when the cache is full.
 * @note This function ensures the cache does not exceed its maximum capacity. It removes
 *       the oldest entry in the LRU list and logs the eviction.
 *
 * @param log A `Logger` instance to record eviction events.
 */
void Cache::cacheUpdate(unique_ptr<Logger>& log){
    while (cache_map.size() >= max_entries && !lru_list.empty()){
        string urlRemove = lru_list.back();
        auto it = cache_map.find(urlRemove);
        // When cache is full, delete the tail
        if (it != cache_map.end()){
            string msg = "evicted" + it->second.response->toString() + " from cache";
            log->log_note(-1, msg);

            delete it->second.response;
            cache_map.erase(it);
        }
        lru_list.pop_back();
    }
}

/**
 * Retrieves a response from the cache.
 * @note This function first attempts a read lock for fast access. If the entry is expired,
 *       it switches to a write lock to remove the entry safely.
 *
 * @param url The URL of the requested resource to be retrieved from cache.
 * @param cache_res Reference to `CacheStatus` that will be updated with the response's cache state.
 * @return A pointer to the cached `Response`, or `NULL` if the entry is not found or expired.
 */
Response* Cache::get(const string&url, CacheStatus& cache_res){
    shared_lock<shared_mutex> lock(cache_mutex);

    auto it = cache_map.find(url);
    if (it == cache_map.end()){
        cache_res = CacheStatus::NOT_IN_CACHE;
        return NULL;
    }

    Response* cac_res = it->second.response;

    if (isExpired(cac_res)) {
        lock.unlock();
        unique_lock<shared_mutex> write_lock(cache_mutex);
        
        it = cache_map.find(url);
        if (it == cache_map.end()) {
            cache_res = CacheStatus::NOT_IN_CACHE;
            return NULL;
        }
        
        cache_res = CacheStatus::EXPIRED;
        return cac_res;
    }

    if(cac_res->getCacheMode() == CACHE_IMMUTABLE){
        lock.unlock();
        unique_lock<shared_mutex> write_lock(cache_mutex);

        it = cache_map.find(url);
        if (it == cache_map.end()) {
            cache_res = CacheStatus::NOT_IN_CACHE;
            return NULL;
        }
        it->second.last_checked = chrono::system_clock::now();

        updateLRU(url);
        cache_res = CacheStatus::VALID;
        return it->second.response;
    }

    if(cac_res->getCacheMode() == CACHE_MUST_REVALIDATE){
        cache_res = CacheStatus::REQUIRES_VALIDATION;
        return cac_res;
    }

    lock.unlock();
    unique_lock<shared_mutex> write_lock(cache_mutex);
    it = cache_map.find(url);
    if (it == cache_map.end()) {
        cache_res = CacheStatus::NOT_IN_CACHE;
        return NULL;
    }
    it->second.last_checked = chrono::system_clock::now();

    updateLRU(url);
    cache_res = CacheStatus::VALID;
    return it->second.response;
}

/**
 * Removes expired cache entries.
 * @note This function iterates through all cached responses and removes entries that
 *       have passed their expiration time.
 *
 * @param log A `Logger` instance to record cache cleaning events.
 */
void Cache::cleanExpiredResponse(unique_ptr<Logger>& log){
    auto now = chrono::system_clock::now();
    last_cleanup = now;

    for (auto it = cache_map.begin(); it != cache_map.end();) {
        if (isExpired(it->second.response)) {
            string url = it->first;
            log->log_note(-1, "Removing expired entry: " + url);
            delete it->second.response;
            lru_list.remove(url);
            it = cache_map.erase(it);
        } else {
            it++;
        }
    }
}

/**
 * Stores a response in the cache.
 * @note If the response has `Cache-Control: no-store`, it will not be stored.
 *       The function ensures expired responses are removed before adding a new entry.
 *
 * @param url The URL of the resource being cached.
 * @param response Pointer to the `Response` object to be stored.
 * @param log A `Logger` instance to record caching events.
 */
void Cache::put(const string& url, Response* response, unique_ptr<Logger>& log){
    if (!response || response->getCacheMode() == CACHE_NO_STORE){
        return;
    }

    unique_lock<shared_mutex> write_lock(cache_mutex);

    auto now = chrono::system_clock::now();
    // If maximum cache clean time has bee reached, clean up the cache
    if (now - last_cleanup > cleanup_interval) {
        cleanExpiredResponse(log);
    }

    auto it = cache_map.find(url);
    if (it != cache_map.end()){
        delete it->second.response;
        it->second.response = response;
        updateLRU(url);
        return;
    }

    cacheUpdate(log);

    CacheEntry new_entry{
        response,
        url
    };

    cache_map[url] = new_entry;
    updateLRU(url);
}

/**
 * Retrieves the number of entries in the cache.
 * @note This function acquires a write lock to ensure thread safety while retrieving the size.
 * @return The number of cached responses.
 */
size_t Cache::size() const {
    unique_lock<shared_mutex> write_lock(cache_mutex);
    return cache_map.size();
}