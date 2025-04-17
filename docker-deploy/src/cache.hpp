#ifndef _CACHE_HPP_
#define _CACHE_HPP_

#include <string>
#include <map>
#include <mutex>
#include <list>
#include <memory>
#include <chrono>
#include <shared_mutex>
#include <unordered_map>
#include "response.hpp"
#include "log.hpp"
#include "util.hpp"

using namespace std;

class Cache {
private:
    struct CacheEntry{
        Response* response;
        string url;
        chrono::system_clock::time_point last_checked;
    };

    unordered_map<string, CacheEntry> cache_map;
    list<string> lru_list;

    const size_t max_entries;
    chrono::seconds cleanup_interval;
    chrono::system_clock::time_point last_cleanup;
    mutable shared_mutex cache_mutex;

    void updateLRU(const string& url);
    void cacheUpdate(unique_ptr<Logger>& log);
    bool isExpired(const Response* response) const;
    void cleanExpiredResponse(unique_ptr<Logger>& log);

public:
    explicit Cache(size_t size, int clean_sec = 300) : max_entries(size), cleanup_interval(clean_sec), last_cleanup(chrono::system_clock::now()) {}
    Response* get(const string&url, CacheStatus &cache_res);
    void put(const string& url, Response* response, unique_ptr<Logger>& log);
    size_t size() const;
};

#endif