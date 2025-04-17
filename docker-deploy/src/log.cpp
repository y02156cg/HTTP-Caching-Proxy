#include "log.hpp"

/**
 * Retrieves the current UTC time in GMT format as a string for logging timestamps.
 * @return A string representing the current time in the format: "Wed Mar 06 12:34:56 2024".
 */
std::string Logger::get_current_time() {
    std::time_t now = std::time(nullptr); //system time
    std::tm *gmtm = std::gmtime(&now); //UTC time in GMT
    std::string time_str = std::asctime(gmtm); //to string
    time_str.pop_back();//remove the newline char
    return time_str;
}

/**
 * Constructs a `Logger` object and opens a log file for writing.
 * The log file is overwritten each time the logger is initialized due to `std::ios::trunc`.
 * @param filename The name of the log file to open.
 *
 * Error Handling: 
 * If fail to open the log_gile, then report error and exit
 */
Logger::Logger(const std::string &filename) {
    log_file.open(filename, std::ios::trunc | std::ios::out);

    if (!log_file.is_open()) {
        std::cerr << "Error opening log file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Destructor that ensures the log file is properly closed.
 * This function is automatically called when the `Logger` object is destroyed.
 */
Logger::~Logger() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

/**
 * Logs a message to the log file with a timestamp.
 * - Uses `std::lock_guard<std::mutex>` to ensure thread safety.
 * - Writes the log entry in the format: `[TIME] message`.
 *
 * @param message The message to log.
 */
void Logger::log(const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;
    log_file << "[" << get_current_time() << "] " << message << std::endl;
}

/**
 * Logs a new request message to the log file with a timestamp.
 * Uses `std::lock_guard<std::mutex>` to ensure thread safety.
 */
void Logger::log_new_request(int request_id, const std::string &request_line, const std::string &ip_from) {
    std::lock_guard<std::mutex> lock(log_mutex);

    if (!log_file.is_open()) return;

    std::string time_str = get_current_time();

    // ID: "REQUEST" from IPFROM @ TIME
    log_file << request_id << ": \"" << request_line
             << "\" from " << ip_from
             << " @ " << time_str << "\n";
    log_file.flush();
}

/**
 * Logs when the proxy forwards a request to the origin server.
 * - Uses `std::lock_guard<std::mutex>` for thread safety.
 * - Writes the log entry in the format:  
 *   `request_id: Requesting "REQUEST" from SERVER`
 * - Calls `flush()` to ensure the log is immediately written to disk.
 *
 * @param request_id The unique ID of the request.
 * @param request_line The HTTP request line.
 * @param server The server to which the request is being sent.
 */
void Logger::log_requesting(int request_id, const std::string &request_line, const std::string &server) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id << ": Requesting \"" << request_line << "\" from " << server << "\n";

    log_file.flush();
}

/**
 * Logs when a response is received from the origin server.
 * - Writes the log entry in the format:  
 *   `request_id: Received "RESPONSE" from SERVER`
 * - Calls `flush()` to ensure the log is immediately written to disk.
 *
 * @param request_id The unique ID of the request.
 * @param response_line The HTTP response line (e.g., `"HTTP/1.1 200 OK"`).
 * @param server The server from which the response was received.
 */
void Logger::log_received(int request_id, const std::string &response_line, const std::string &server) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id << ": Received \"" << response_line << "\" from " << server << "\n";
    log_file.flush();
}

/**
 * Logs the cache status when a client requests a resource.
 *  Logs different cache statuses:
 *   - `"not in cache"`
 *   - `"in cache, but expired at <time>"`
 *   - `"in cache, requires validation"`
 *   - `"in cache, valid"`
 *
 * @param request_id The unique ID of the request.
 * @param status The cache status (e.g., `NOT_IN_CACHE`, `EXPIRED`, `REQUIRES_VALIDATION`, `VALID`).
 * @param reason_or_expire The reason for the cache status or the expiration time.
 */
void Logger::log_cache_request(int request_id, CacheStatus status, const std::string &reason_or_expire) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    switch (status) {
        case CacheStatus::NOT_IN_CACHE:
            log_file << request_id << ": not in cache " << reason_or_expire << "\n";
            break;
        case CacheStatus::EXPIRED:
            log_file << request_id << ": in cache, but expired at " << reason_or_expire << "\n";
            break;
        case CacheStatus::REQUIRES_VALIDATION:
            log_file << request_id << ": in cache, requires validation\n";
            break;
        case CacheStatus::VALID:
            log_file << request_id << ": in cache, valid\n";
            break;
        default:
            break;
    }
    log_file.flush();
}

/**
 * Logs caching decisions when storing a response.
 * - Logs messages indicating:
 *   - `"not cacheable because <reason>"`
 *   - `"cached, expires at <time>"`
 *   - `"cached, but requires re-validation"`
 *
 * @param id The unique ID of the request.
 * @param status The cache storage decision (`NOT_CACHEABLE`, `WILL_EXPIRE`, `REVALIDATION`).
 * @param reason_or_expire The reason the response is not cacheable or its expiration time.
 */
void Logger::log_cache_response(int id, CacheStatus status, const std::string &reason_or_expire) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    switch (status) {
        case CacheStatus::NOT_CACHEABLE:
            log_file << id << ": not cacheable because " << reason_or_expire << "\n";
            break;
        case CacheStatus::WILL_EXPIRE:
            log_file << id << ": cached, expires at " << reason_or_expire << "\n";
            break;
        case CacheStatus::REVALIDATION:
            log_file << id << ": cached, but requires re-validation\n";
            break;
        default:
            break;
    }
    log_file.flush();
}

/**
 * Logs when the proxy sends a response to the client.
 * - Writes the log entry in the format:  
 *   `request_id: Responding "RESPONSE"`
 *
 * @param request_id The unique ID of the request.
 * @param response_line The HTTP response line being sent (e.g., `"HTTP/1.1 200 OK"`).
 */
void Logger::log_responding(int request_id, const std::string &response_line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id << ": Responding \"" << response_line << "\"\n";
    log_file.flush();
}

/**
 * Logs when a proxy tunnel (for `CONNECT` requests) is closed.
 * - Writes the log entry in the format:  
 *   `request_id: Tunnel closed`
 *
 * @param request_id The unique ID of the tunnel request.
 */
void Logger::log_tunnel_closed(int request_id) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id << ": Tunnel closed\n";
    log_file.flush();
}

/**
 * Logs an error message for a specific request.
 * - Writes the log entry in the format:  
 *   `request_id: ERROR error_message`
 *
 * @param request_id The unique ID of the request.
 * @param error_message The error message to log.
 */
void Logger::log_error(int request_id, const std::string &error_message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id <<": ERROR " << error_message << "\n";
}

/**
 * Logs a general note message for a specific request.
 * - Writes the log entry in the format:  
 *   `request_id: NOTE error_message`
 *
 * @param request_id The unique ID of the request.
 * @param error_message The note message to log.
 */
void Logger::log_note(int request_id, const std::string &error_message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file.is_open()) return;

    log_file << request_id <<": NOTE " << error_message << "\n";
}