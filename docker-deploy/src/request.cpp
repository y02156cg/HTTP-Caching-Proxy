#include "request.hpp"

/**
 * Constructs a Request object from a raw HTTP request string.
 * Initialize all properties with empty string
 */
Request::Request(const std::string& httpRequest) : httpRequest(httpRequest) {
    // Initialize with empty values
    method = "";
    requestHeader = "";
    host = "";
    userAgent = "";
    url = "";
    connection = "";
    port = "";
    IfNoneMatch = "";
    IfModifiedSince = "";
}

/**
 * Parses the raw HTTP request string and extracts relevant fields,
 *       such as `Host`, `User-Agent`, `Connection`, `If-None-Match`, and `If-Modified-Since`.
 */
void Request::parseRequest(){
    istringstream ss(httpRequest);
    string line;
    bool isheader = true;

    while(getline(ss, line)){
        if(!line.empty() && line.back() == '\r'){
            line.pop_back();
        }

        if(line.empty()){break;}

        // Set header line first
        if(isheader){
            setHeader(line);
            isheader = false;
        } else if (line.find(HOST) == 0){
            setHostnameAndPort(line);
        } else if (line.find(USERAGENT) == 0){
            userAgent = line.substr(line.find(":") + 1);
            userAgent.erase(0, userAgent.find_first_not_of(" "));
        } else if (line.find(CONNECTION) == 0){
            connection = line.substr(line.find(":") + 1);
            connection.erase(0, connection.find_first_not_of(" "));
        } else if (line.find(IFNONEMATCH) == 0){
            IfNoneMatch = line.substr(line.find(":") + 1);
            IfNoneMatch.erase(0, IfNoneMatch.find_first_not_of(" "));
        } else if (line.find(IFMODIFIED) == 0){
            IfModifiedSince = line.substr(line.find(":") + 1);
            IfModifiedSince.erase(0, IfModifiedSince.find_first_not_of(" "));
        }
    }
}

/**
 * Extracts the HTTP method, URL, and HTTP version from the request line.
 * This function updates the `method`, `url`, and extracts the HTTP version for internal processing.
 * 
 * @param line The first line of the HTTP request.
 */
void Request::setHeader(const string& line){
    requestHeader = line;
    istringstream ss(line);
    string http_version;

    ss >> method >> url >> http_version;
}

/**
 * Extracts the hostname and port from the `Host` header.
 * If no port is found, it defaults to an empty string.
 * @param line The `Host` header line from the HTTP request.
 */
void Request::setHostnameAndPort(const string& line){
    string hostHeader = line.substr(line.find(":") + 1);
    hostHeader.erase(0, hostHeader.find_first_not_of(" "));

    size_t colonPos = hostHeader.find(":");
    if (colonPos != std::string::npos) { // find port in this line
        host = hostHeader.substr(0, colonPos);
        port = hostHeader.substr(colonPos + 1);
    } else {
        host = hostHeader;
    }
}

/**
 * Generate a new request line and headers in a valid HTTP format including the newly gotten properties
 *
 * @return A string representing the reconstructed HTTP request.
 */
string Request::Request_line() const {
    string request_line;
    
    request_line += method + " " + url + " HTTP/1.1\r\n";
    
    if (!host.empty()) {
        request_line += HOST + host;
        if (!port.empty() && port != "80") {
            request_line += ":" + port;
        }
        request_line += "\r\n";
    }
    
    if (!userAgent.empty()) {
        request_line += USERAGENT + userAgent + "\r\n";
    }
    
    if (!connection.empty()) {
        request_line += CONNECTION + connection + "\r\n";
    }
    
    if (!IfNoneMatch.empty()) {
        request_line += IFNONEMATCH + IfNoneMatch + "\r\n";
    }
    
    if (!IfModifiedSince.empty()) {
        request_line += IFMODIFIED + IfModifiedSince + "\r\n";
    }
    
    // End with an empty line to conclude the header section
    request_line += "\r\n";
    
    return request_line;
}
