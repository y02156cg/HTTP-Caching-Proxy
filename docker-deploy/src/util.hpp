/**
 * @file util.hpp
 * Defines constants and enumerations for handling HTTP caching and headers.
 *
 * @details This file contains various string constants representing HTTP headers and
 *          caching directives, as well as enumerations for cache states. It is used
 *          throughout the proxy server to manage caching, response validation, and 
 *          request handling.
 *
 * @section Caching Constants
 * - `CACHECTR_NO_STORE`: Indicates that the response should not be cached.
 * - `CACHECTR_NO_CACHE`: Requires cache revalidation before reuse.
 * - `CACHECTR_REVALIDATE` & `CACHECTR_PROXY_REVALIDATE`: Forces revalidation with the origin server.
 * - `CACHECTR_PRIVATE` & `CACHECTR_PUBLIC`: Specifies whether the cache is private or public.
 * - `CACHECTR_MAXAGE` & `CACHECTR_SMAXAGE`: Defines the maximum lifetime of a cached response.
 *
 * @section HTTP Header Constants
 * - `HEADER_TRANSFER`: Specifies transfer encoding (e.g., "Transfer-Encoding: chunked").
 * - `HEADER_CONTENT_LEN`: Represents "Content-Length" for response bodies.
 * - `HEADER_DATE`, `HEADER_EXPIRE`, `HEADER_LAST_MODIFY`, `HEADER_ETAG`, `HEADER_CACHECTRL`: 
 *   Various headers related to HTTP response metadata and caching.
 *
 * @section Cache Modes
 * - `CACHE_PUBLIC`, `CACHE_PRIVATE`: Defines cache visibility.
 * - `CACHE_IMMUTABLE`, `CACHE_MUST_REVALIDATE`, `CACHE_NO_STORE`, `CACHE_NORMAL`: Defines caching behavior.
 *
 * @section Cache Status Enum
 * - `CacheStatus::NOT_IN_CACHE`: Response is not in cache.
 * - `CacheStatus::EXPIRED`: Cached response has expired.
 * - `CacheStatus::REQUIRES_VALIDATION`: Response must be revalidated before use.
 * - `CacheStatus::VALID`: Cached response is still valid.
 * - `CacheStatus::NOT_CACHEABLE`: Response cannot be cached.
 * - `CacheStatus::WILL_EXPIRE`: Response will expire soon.
 * - `CacheStatus::REVALIDATION`: Response requires revalidation.
 *
 * @section 
 * - `HOST`, `USERAGENT`, `CONNECTION`, `IFNONEMATCH`, `IFMODIFIED`: 
 *   Common request headers used for HTTP communication and cache validation.
 *
 */
#ifndef _UTIL_HPP_
#define _UTIL_HPP_

const char * const CACHECTR_NO_STORE = "no-store";
const char * const CACHECTR_NO_CACHE = "no-cache";
const char * const CACHECTR_REVALIDATE = "must-revalidate";
const char * const CACHECTR_PROXY_REVALIDATE = "proxy-revalidate";
const char * const CACHECTR_PRIVATE = "private";
const char * const CACHECTR_PUBLIC = "public";
const char * const CACHECTR_MAXAGE = "max-age=";
const char * const CACHECTR_SMAXAGE = "s-maxage=";

const char * const HEADER_TRANSFER = "Transfer-Encoding";
const char * const HEADER_CHUNCK = "chunked";
const char * const HEADER_CONTENT_LEN = "Content-Length";

const char * const HEADER_DATE = "Date";
const char * const HEADER_EXPIRE = "Expires";
const char * const HEADER_LAST_MODIFY = "Last-Modified";
const char * const HEADER_ETAG = "ETag";
const char * const HEADER_CACHECTRL= "Cache-Control";

#define CACHE_PUBLIC 1
#define CACHE_PRIVATE 2

#define CACHE_IMMUTABLE 3
#define CACHE_MUST_REVALIDATE 4
#define CACHE_NO_STORE 5
#define CACHE_NORMAL 6

enum class CacheStatus {
    NOT_IN_CACHE = 7, //not in cache
    EXPIRED = 8,  //in cache, but expired at EXPIREDTIME
    REQUIRES_VALIDATION = 9, //in cache, requires validation
    VALID = 10,  //in cache, valid
    NOT_CACHEABLE = 11,
    WILL_EXPIRE = 12,
    REVALIDATION = 13
};

const char * const HOST = "Host: ";
const char * const USERAGENT = "User-Agent: ";
const char * const CONNECTION = "Connection: ";
const char * const IFNONEMATCH = "If-None-Match: ";
const char * const IFMODIFIED = "If-Modified-Since: ";

#endif