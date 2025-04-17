#include "response.hpp"

/**
 * Parse an HTTP date string and converts it to a chrono::system_clock::time_point
 * @param http_date A string containing an HTTP date in the format: "Wed, 21 Oct 2025 07:28:00 GMT".
 * @return `chrono::system_clock::time_point` representing the parsed time.
 */
chrono::system_clock::time_point Response::parseHttpDate(const string& http_date){
    tm tm = {};
    istringstream ss(http_date);
    ss >> get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");//parse time based on HTTP format
    
    //Convert tm structure to time_t, then to chrono::system_clock::time_point
    auto t = mktime(&tm);
    auto converted_t = chrono::system_clock::from_time_t(t);
    return converted_t;
}

/**
 * Formats a `chrono::system_clock::time_point` into an HTTP date string.
 * @param tp A `chrono::system_clock::time_point` representing the time to be formatted.
 * @return A string containing the formatted date in the standard HTTP format:
 *         "Wed, 21 Oct 2015 07:28:00 GMT"
 */
string Response::formatHTTPDate(const chrono::system_clock::time_point& tp){
    auto time_t = chrono::system_clock::to_time_t(tp);
    stringstream ss;
    ss << put_time(gmtime(&time_t), "%a, %d %b %Y %H:%M:%S GMT");
    return ss.str();
}

/**
 * Calculates the time difference between two HTTP date strings
 * @return The time difference in seconds as a `long long`
 */
long long Response::timeDifference(const string& time1, const string& time2){
    auto t1 = parseHttpDate(time1);
    auto t2 = parseHttpDate(time2);
    return chrono::duration_cast<chrono::seconds>(t2 - t1).count();
}

/**
 * Assign cache mode and cache visiblity according to `Cache-Control` header in response
 * Processes the `Cache-Control` header field in an HTTP response
 *       and sets caching behavior based on directives like `no-store`, `no-cache`,
 *       `must-revalidate`, `private`, `public`, and `max-age`.
 * Exception Handling:
 * - The function catches exceptions when parsing `max-age` or `s-maxage` values.
 * - If `stoi(directive.substr(8))` or `stoi(directive.substr(9))` throws an exception 
 *   (e.g., due to a non-numeric value), `max_age` is set to `-1`, indicating an invalid or missing value.
 * - The function also ensures that `s-maxage` only overrides `max-age` if the response is `public`.
 *
 * If no restrictive directives (`no-store`, `no-cache`, `must-revalidate`) are present,
 * the response is considered `CACHE_NORMAL` and can be stored and reused normally.
 */
void Response::parseCacheControl(){
    auto it = header.find("Cache-Control");
    if (it == header.end()) return;

    istringstream iss(it->second);
    string directive;
    bool issmax_age = false;

    while(getline(iss, directive, ',')){
        directive.erase(0, directive.find_first_not_of(" "));
        directive.erase(directive.find_last_not_of(" ") + 1);

        if(directive == CACHECTR_NO_STORE){
            no_store = true;
            cache_mode = CACHE_NO_STORE;
        } else if (directive == CACHECTR_NO_CACHE){
            no_cache = true;
            cache_mode = CACHE_MUST_REVALIDATE;
        } else if (directive == CACHECTR_REVALIDATE || directive == CACHECTR_PROXY_REVALIDATE){
            must_revalidate = true;
            cache_mode = CACHE_MUST_REVALIDATE;
        } else if (directive == CACHECTR_PRIVATE){
            cache_visibility = CACHE_PRIVATE;
        } else if (directive == CACHECTR_PUBLIC){
            cache_visibility = CACHE_PUBLIC;
        } else if (directive.find(CACHECTR_MAXAGE) == 0){
            try{
                if (issmax_age == false){
                    max_age = stoi(directive.substr(8));
                }
            } catch (const exception& e){
                max_age = -1;
            }
        } else if (directive.find(CACHECTR_SMAXAGE)){
            try{
                if (cache_visibility == CACHE_PUBLIC){
                    issmax_age = true;
                    max_age = stoi(directive.substr(9));
                }
            } catch (const exception& e){
                max_age = -1;
            }
        }
    }

    if (!no_cache && !no_cache && !must_revalidate){
        cache_mode = CACHE_NORMAL;
    }
}

/**
 * Parses an HTTP response string and sets relevant fields and get cache mode and expire time
 * 
 * @param httpResponse The raw HTTP response string received from a server.
 *
 * Exception Handling:
 * - Malformed Response Handling:
 *   - If the response string is empty or malformed, a `runtime_error` is thrown.
 *   - If the `status_line` cannot be read, the function throws `runtime_error("Error: Status line cannot be read.")`.
 * - Invalid Header Values:
 *   - If `Content-Length` contains a non-numeric value, `stoi(value)` may throw `std::invalid_argument` or `std::out_of_range`,
 *     causing an exception to be thrown.
 *   - The `catch` block ensures that `response` is properly deallocated before rethrowing the exception.
 */
void Response::parseResponse(const string& httpResponse){
    Response* response = new Response();
    try{
        istringstream iss(httpResponse);
        string status_line;

        if (!getline(iss, status_line)){
            delete response;
            throw runtime_error("Error: Status line cannot be read.");
        }

        istringstream status_stream(status_line);
        status_stream >> http_version >> status_code; // Assign http version and status code
        getline(status_stream, status_message);

        string line;
        // Get headers related attributes and their corresponding values, stored in a map variable
        while(getline(iss, line) && line != "\r" && line != ""){
            size_t colomn = line.find(':');
            if(colomn != string::npos){
                string key = line.substr(0, colomn);
                string value = line.substr(colomn + 1);
                value.erase(0, value.find_first_not_of(" "));
                value.erase(value.find_last_not_of("\r") + 1);
                header[key] = value;

                // Set is_chunked flag
                if(key == HEADER_TRANSFER && value.find(HEADER_CHUNCK) != string::npos){
                    is_chunked = true;
                } else if (key == HEADER_CONTENT_LEN){
                    content_length = stoi(value); // Get content length
                }
            }
        }

        if(!is_chunked){
            stringstream body_stream;
            while (getline(iss, line)){
                body_stream << line << "\n";
            }
            body = body_stream.str();
        }

        parseCacheControl();
        setExpiredTime();
    }
    // Catch the exceptions and delete the wrong response
    catch (const exception& e){
        delete response;
        throw;
    }
}

/**
 * Sets the expiration time for the response based on HTTP headers.
 *
 * This function determines the cache expiration time using the following priority:
 *       1. If `Cache-Control: max-age` is present, expiration is calculated based on the `Date` header.
 *       2. If `Expires` header is present, it is used directly.
 *       3. If `must-revalidate` is set and `Date` exists, `Date` is used as expiration.
 *       4. If `Last-Modified` is available, heuristic expiration is estimated as `(response_date - last_modified) / 10`.
 */
void Response::setExpiredTime(){
    auto date_it = header.find(HEADER_DATE);
    
    // When max age is specified for the response
    if (date_it != header.end() && max_age > 0){
        auto responseDate = parseHttpDate(date_it->second);
        // expire time = current time + max-age
        auto expiredTime = responseDate + chrono::seconds(max_age);
        expire_time = formatHTTPDate(expiredTime);
    }
    // When max-age is not found
    else{
        auto expire_it = header.find(HEADER_EXPIRE);
        if (expire_it != header.end()){
            expire_time = expire_it->second;
        } 
        // When revalidate is needed then expired time is the header date
        else if (must_revalidate && date_it != header.end()){
            expire_time = date_it->second;
        } 
        // When expire time is not specified and last modify time is found, then make prediction based on heuristic expire time
        else if (cache_mode != CACHE_NO_STORE && header.find(HEADER_LAST_MODIFY) != header.end() && date_it != header.end()){
            long long time_diff = timeDifference(header[HEADER_LAST_MODIFY], date_it->second);
            long long heuristic_expire = time_diff / 10; // expire time is 10% of the time difference

            auto responseDate = parseHttpDate(date_it->second);
            auto expire_t = responseDate + chrono::seconds(heuristic_expire);
            expire_time = formatHTTPDate(expire_t);
        }
    }
}
/**
 * Appends chunked transfer encoding data to the response body.
 *
 * @param chunk_data A `vector<char>` containing a chunk of response data.
 *
 * This function is used when handling HTTP responses with `Transfer-Encoding: chunked`.
 *       If the response is chunked (`is_chunked == true`), the provided `chunk_data` is appended
 *       to the `body` of the response.
 */
void Response::addChunkedData(const vector<char>& chunk_data){
    if (is_chunked){
        body.append(chunk_data.begin(), chunk_data.end());
    }
}

/**
 * Appends response body data and updates `Content-Length` header.
 *
 * @param response_body A `string` containing the response body to append.
 *
 * This function is used when handling non-chunked responses. It appends 
 * the provided `response_body` to the `body` and updates the `Content-Length` header.
 */
void Response::addResponseBody(const string& response_body){
    body += response_body;
    header[HEADER_CONTENT_LEN] = to_string(body.length());
}

/* Response fields getters */
int Response::getStatusCode() const { return status_code; }
const string& Response::getStatusMessage() const { return status_message; }
const string& Response::getHttpVersion() const { return http_version; }
const std::map<std::string, std::string>& Response::getHeaders() const { return header; }
const std::string& Response::getBody() const { return body; }
bool Response::getIsChunked() const { return is_chunked; }
int Response::getContentLength() const { return content_length; }
const std::string& Response::getExpireTime() const { return expire_time; }
int Response::getCacheMode() const {return cache_mode;}

/* Cache control getters */
bool Response::getNoStore() const { return no_store; }
bool Response::getNoCache() const { return no_cache; }
bool Response::getMustRevalidate() const { return must_revalidate; }
int Response::getMaxAge() const {return max_age;}

/** Retrieve header value from the response 
 * @return A string containing the corresponding value of the header.
 *         Returns an empty string if the header is not present.
 */
string Response::getDate() const {
    auto it = header.find(HEADER_DATE);
    return (it != header.end()) ? it->second : "";
}

string Response::getExpires() const {
    auto it = header.find(HEADER_EXPIRE);
    return (it != header.end()) ? it->second : "";
}

string Response::getETag() const {
    auto it = header.find(HEADER_ETAG);
    return (it != header.end()) ? it->second : "";
}

string Response::getLastModified() const {
    auto it = header.find(HEADER_LAST_MODIFY);
    return (it != header.end()) ? it->second : "";
}

string Response::getCacheControl() const {
    auto it = header.find(HEADER_CACHECTRL);
    return (it != header.end()) ? it->second : "";
}

string Response::getTransferEncoding() const {
    auto it = header.find(HEADER_TRANSFER);
    return (it != header.end()) ? it->second : "";
}

/**
 * Determines whether the response is cacheable based on HTTP caching rules.
 * @param isPrivateCache A boolean indicating whether the cache is private.
 * @return `true` if the response can be cached, `false` otherwise.
 * @note A response is considered cacheable if:
 *       - The status code is `200 OK`.
 *       - And the `Cache-Control: no-store` directive is not set.
 *       - If the response is `private`, it can only be cached in a private cache.
 */
bool Response::isCacheable(bool isPrivateCache) const {
    if (status_code != 200 || cache_mode == CACHE_NO_STORE) {
        return false;
    }

    if (cache_visibility == CACHE_PRIVATE && !isPrivateCache) {
        return false;
    }

    return true;
}

/**
 * Determines whether the response requires revalidation before reuse.
 * @return `true` if the response must be revalidated before being served from the cache,
 *         `false` otherwise.
 * @note A response requires revalidation if:
 *       - The `Cache-Control: must-revalidate` directive is present.
 *       - Or the `Cache-Control: no-cache` directive is set.
 */
bool Response::needsRevalidation() const {
    return cache_mode == CACHE_MUST_REVALIDATE || no_cache;
}

/**
 * Convert response to string for sending
 */ 
string Response::toString() const {
    std::stringstream ss;
    ss << http_version << " " << status_code << " " << status_message << "\r\n";
    
    // Get the rest of the response with their header and content
    for (const auto& header_pair : header) {
        ss << header_pair.first << ": " << header_pair.second << "\r\n";
    }
    
    // Add rest body to the content
    ss << "\r\n" << body;
    return ss.str();
}