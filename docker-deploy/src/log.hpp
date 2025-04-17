#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <mutex>
#include <sys/stat.h>
#include "util.hpp"

class Logger {
private:
    std::ofstream log_file;  
    std::mutex log_mutex;   

    std::string get_current_time();

public:
    explicit Logger(const std::string &filename);

    ~Logger();

    void log(const std::string &message);

    /*
    Upon receiving a new request, 
    - the proxy should assign it a unique id (ID), 
    - print the ID, time received (TIME)
    - IP address the request was received from (IPFROM)
    - the HTTP request line (REQUEST)

    ID: "REQUEST" from IPFROM @ TIME
    */
    void log_new_request(int request_id, const std::string &request_line, const std::string &ip_from);

    // ID: Requesting "REQUEST" from SERVER
    void log_requesting(int request_id, const std::string &request_line, const std::string &server);

    // ID: Received "RESPONSE" from SERVER
    void log_received(int request_id, const std::string &response_line, const std::string &server);

    /*
    If your proxy receives a 200-OK in response to a GET request, it should print one of the following:
    ID: not cacheable because REASON
    ID: cached, expires at EXPIRES
    ID: cached, but requires re-validation
    */
    void log_cache_request(int request_id, CacheStatus status, const std::string &reason_or_expire = "");
    void log_cache_response(int id, CacheStatus status, const std::string & reason_or_expire = "");

    // ID: Responding "RESPONSE"
    void log_responding(int request_id, const std::string &response_line);

    // ID: Tunnel closed
    void log_tunnel_closed(int request_id);

    //ID: ERROR MESSAGE
    void log_error(int request_id, const std::string &error_message);

    //ID: NOTE MESSAGE
    void log_note(int request_id, const std::string &error_message);
};

#endif 
