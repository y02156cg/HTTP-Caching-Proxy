#ifndef _RESPONSE_HPP_
#define _RESPONSE_HPP_

#include <cstdio>
#include <map>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>
#include <chrono>
#include <iomanip> 
#include "util.hpp"

using namespace std;

class Response{
private:
    int status_code{0};
    string status_message;
    string http_version;
    map<string, string> header;
    string body;

    chrono::system_clock::time_point received_time;
    string expire_time;

    bool is_chunked{false};
    int content_length{-1};

    bool no_store{false};
    bool no_cache{false};
    bool must_revalidate{false};
    int max_age{-1};
    int cache_mode{0};
    int cache_visibility{CACHE_PUBLIC};

    void parseCacheControl();
    chrono::system_clock::time_point parseHttpDate(const string& http_date);
    string formatHTTPDate(const chrono::system_clock::time_point& tp);
    long long timeDifference(const string& time1, const string& time2);

public:
    void parseResponse(const string& httpResponse);
    void setExpiredTime();
    void addChunkedData(const vector<char>& chunk_data);
    void addResponseBody(const string& response_body);
    int getStatusCode() const;
    const std::string& getStatusMessage() const;
    const std::string& getHttpVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string& getBody() const;
    bool getIsChunked() const;
    int getContentLength() const;
    int getCacheMode() const;
    const std::string& getExpireTime() const;
    int getMaxAge() const;
    string getDate() const;
    string getExpires() const;
    string getETag() const;
    string getLastModified() const;
    string getCacheControl() const;
    string getTransferEncoding() const;

    bool getNoStore() const;
    bool getNoCache() const;
    bool getMustRevalidate() const;

    bool isCacheable(bool isPrivateCache = false) const;
    bool needsRevalidation() const;
    string toString() const;


};

#endif

