#include "proxy.hpp"

/**
 * Generates a unique request ID for tracking and logging each HTTP request processed by the proxy.
 * Increment every time new request is made
 * @return An integer representing the next available request ID.
 */
int Proxy::generateRequestID(){
    return request_count++;
}

/**
 * Receives data from a socket (both cliend and server) with a specified timeout.
 *
 * @param socket_fd The file descriptor of the socket to read from.
 * @param timeout The timeout value (in seconds) for waiting on data.
 * @return A string containing the received data.
 *
 * @details
 * - Uses `poll()` to wait until data is available for reading or the timeout is reached.
 * - Reads data in chunks using `recv()` until no more data is available.
 * - Handles errors such as:
 *   - `poll()` failure (`rv == -1`), which throws a `runtime_error`.
 *   - `recv()` failure (`bytes_received < 0`), which logs the error message.
 *   - Connection closure (`bytes_received == 0`), which logs a message.
 *
 * @throws `std::runtime_error` if `poll()` fails.
 */
string Proxy::receiveFromSocket(int socket_fd, int timeout){
    string received_data;
    char buffer[65536];

    struct pollfd fd;
    fd.fd = socket_fd;
    fd.events = POLLIN;

    while(true){
        int rv = poll(&fd, 1, timeout * 1000); // wait for socket to receive data

        if(rv == -1){
            throw runtime_error("Error in poll");
        } else if(rv == 0){
            break;
        }

        int bytes_received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received < 0) {
        perror("recv");  // Print exact error
        cerr << "Error receiving data from socket (errno: " << errno << " - " << strerror(errno) << ")" << endl;
        break;
    } else if (bytes_received == 0) {
        cerr << "Socket closed by remote server.\n" << received_data << endl;
        break;
    }

        buffer[bytes_received] = '\0';
        // When new contents are received, append to the total received data
        received_data.append(buffer, bytes_received);

        if(bytes_received < (int)sizeof(buffer) - 1){
            break;
        }
    }
    return received_data;
}

/**
 * Handles receiving and forwarding a chunked HTTP response
 * If response is chunked, send the response segment by segment to the client
 * @param server_fd The socket file descriptor connected to the origin server.
 * @param client_fd The socket file descriptor connected to the client.
 * @return A `vector<char>` containing the complete chunked response received from the server.
 *
 * @details
 * - Reads data from the `server_fd` in chunks using `recv()`.
 * - Stores received data in the `response` vector and forwards it to `client_fd`.
 * - Checks for the termination sequence `"0\r\n\r\n"` to detect the end of a chunked response.
 * - Stops reading when the connection is closed or an error occurs.
 */
vector<char> Proxy::handleChunkResponse(int server_fd, int client_fd){
    vector<char> response; // vector to store binary reponse characters
    char buffer[65536];
    int byte_received;

    while(true){
        memset(buffer, 0, sizeof(buffer));
        byte_received = recv(server_fd, buffer, sizeof(buffer), 0); // receive from server

        if(byte_received <= 0){
            break;
        }

        response.insert(response.end(), buffer, buffer + byte_received);
        send(client_fd, buffer, byte_received, 0); // send to client immediately after received

        // Test if end of the chunked response is received, if so, then break receiving loop
        if(response.size() >= 5){
            string end(response.end() - 5, response.end());
            if(end == "0\r\n\r\n"){
                break;
            }
        }
    }

    return response;
}

/**
 * Handles receiving a long HTTP response from the server.
 *
 * @param server_fd The socket file descriptor connected to the origin server.
 * @return A `vector<char>` containing the full response received from the server.
 *
 * @details
 * - Reads data from `server_fd` using `recv()` in a loop until no more data is available.
 * - Stores received data in the `response` vector.
 * - Stops reading when the connection is closed or an error occurs.
 */
vector<char> Proxy::handleLongResponse(int server_fd){
    vector<char> response;
    char buffer[65536];
    int byte_received;

    while(true){
        memset(buffer, 0, sizeof(buffer));
        byte_received = recv(server_fd, buffer, sizeof(buffer), 0);

        if(byte_received <= 0){break;}
        response.insert(response.end(), buffer, buffer + byte_received);
    }

    return response;
}

/**
 * Establishes a connection to the origin server (as a client).
 * - Uses `getaddrinfo()` to resolve the server's address information.
 * - Iterates through the resolved addresses, attempting to create and connect a socket.
 * - If a connection attempt fails, it tries the next available address.
 * - Sets a **10-second receive timeout** on the socket using `setsockopt()`.
 * - Returns `server_fd` on success, otherwise logs an error and returns `-1`.
 *
 * @param host The hostname or IP address of the server to connect to.
 * @param port The port number to connect to on the server.
 * @return The socket file descriptor (`server_fd`) for the connected server, or `-1` if the connection fails.
 */
int Proxy::connectServer(const string& host, int port){
    struct addrinfo server_info, *server_info_list, *p;
    int server_fd;

    memset(&server_info, 0, sizeof(server_info));
    server_info.ai_family = AF_UNSPEC;
    server_info.ai_socktype = SOCK_STREAM;

    string port_str = to_string(port);

    int status = getaddrinfo(host.c_str(), port_str.c_str(), &server_info, &server_info_list); // server_info a link list of server addr
    if (status != 0) {
        logger->log_error(-1, "Failed to get address info: " + std::string(gai_strerror(status))); 
        return -1;
    }

    for(p = server_info_list; p != NULL; p = p->ai_next){
        server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(server_fd == -1){continue;}

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0; // no additional microseconds
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // When the client fail to connect to the server, close the file descriptor and 
        if(connect(server_fd, p->ai_addr, p->ai_addrlen) == -1){
            close(server_fd);
            continue;
        }

        // When connection successful close the loop
        break;
    }

    freeaddrinfo(server_info_list);
    if(p == NULL){
        logger->log_error(-1, "Failed to connect to " + host + ":" + port_str);
        return -1;
    }
    return server_fd;
}


/**
 * Log cache status response according to cached response content
 * - Checks whether the response is **cacheable**.
 * - If not cacheable, logs the reason (`no-store`, `status code not 200`, etc.), and delete the response to prevent memory leaks.
 * - If the response has an expiration time (`Expires`, `Cache-Control: max-age`), logs it.
 * - If `must-revalidate` or `no-cache` is present, logs that revalidation is required.
 * - If cacheable, stores the response in the cache using `cache.put(url, response, logger)`.
 *
 * @param response Pointer to the `Response` object received from the server.
 * @param url The URL associated with the response.
 * @param request_id The unique request identifier for logging.
 */
void Proxy::handleCaching(Response* response, const string& url, int request_id){
    if(!response->isCacheable()){
        string reason;
        if(response->getStatusCode() != 200){
            reason = "status code is not 200 OK";
        } else if(response->getNoStore()){
            reason = "no-store directive";
        } else if(response->getCacheMode() == CACHE_NO_STORE){
            reason = "cache-control: no-store";
        } else{
            reason = "unknow";
        }

        logger->log_cache_response(request_id, CacheStatus::NOT_CACHEABLE, reason);
        delete response;
        return;
    }

    if (!response->getExpireTime().empty()) {
        logger->log_cache_response(request_id, CacheStatus::WILL_EXPIRE, response->getExpireTime());
    } else if (response->getNoCache() || response->getMustRevalidate()) {
        logger->log_cache_response(request_id, CacheStatus::REVALIDATION);
    }

    // Put this response into the cache
    cache.put(url, response, logger);
}

/**
 * Sends an error response to the client.
 * - Constructs an HTML error response with the given `status_code` and `reason`.
 * - Sends the error response to the client.
 * - Logs the error response using `logger->log_responding()`.
 *
 *  * When corrupted response is received, reply with 502 error code
 *  * When a malformed request is received, reply with 400 error code
 * @param client_fd The client socket file descriptor.
 * @param status_code The HTTP status code to return (e.g., 400, 502).
 * @param reason A string describing the error (e.g., `"Bad Gateway"`).
 */
void Proxy::sendErrorResponse(int client_fd, int status_code, const string& reason){
    string status_line = "HTTP/1.1 " + std::to_string(status_code) + " " + reason;
    string response = status_line + "\r\n";
    response += "Content-Type: text/html\r\n";
    response += "Connection: close\r\n";

    std::string body = "<html><head><title>" + std::to_string(status_code) + " " + 
                           reason + "</title></head><body><h1>" + 
                           std::to_string(status_code) + " " + reason + 
                           "</h1><p>Proxy Error</p></body></html>";

    response += "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n";
    response += body;

    send(client_fd, response.c_str(), response.length(), 0);
    logger->log_responding(-1, status_line);
}

/**
 * Handles an HTTP request received from a client.
 * - Receives the HTTP request from the client.
 * - Parses the request into a `Request` object.
 * - Generates a unique request ID for logging.
 * - Calls the appropriate request handler based on the HTTP method (`GET`, `POST`, `CONNECT`).
 * - If an unsupported method is received, returns a `501 Not Implemented` error.
 * - Catches and logs any exceptions that occur during request processing.
 *
 * If an error occurs while parsing the request, a `400 Bad Request` error is sent to the client.
 * 
 * @param client_fd The client socket file descriptor.
 * @param client_addr The `sockaddr_in` structure containing the client's address.
 */
void Proxy::receiveClient(int client_fd, struct sockaddr_in client_addr){
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

    try{
        string http_request = receiveFromSocket(client_fd); // Receive request from client

        if(http_request.empty()){
            logger->log_error(-1, "Empty request received"); 
            close(client_fd);
            return;
        }

        // Create a Request class object to record request properties and process request
        Request request(http_request); 
        try{
            request.parseRequest(); // Parse request string to get all request property contents
        } catch(const exception& e){
            // When exception happens, it first log the error into the log
            // Reply to client with the error code 400 to indicate the request is bad
            // Finally close the client file descriptor
            logger->log_error(-1, string("Fail to parse request"));
            sendErrorResponse(client_fd, 400, "Bad Request");
            close(client_fd);
            return;
        }

        int request_id = generateRequestID();
        logger->log_new_request(request_id, client_ip, request.requestHeader); // log a new request

        if(request.method == "GET"){
            processGet(client_fd, request, request_id);
        } else if(request.method == "POST"){
            processPost(client_fd, request, request_id);
        } else if(request.method == "CONNECT"){
            processConnect(client_fd, request, request_id);
        } else{
            // When the method is not found from the three required method
            logger->log_error(request_id,  "Method " + request.method + " not found"); 
            sendErrorResponse(client_fd, 501, "Not implement method request");
            close(client_fd);
        }
    } catch(const exception& e){ // Catch exception for the whole client request handling process
        // Log the error and print it out
        // close the client file descriptor
        logger->log_error(-1, std::string("Unhandled exception: ") + e.what());
        close(client_fd);
    }
}

/**
 * Handles an HTTP GET request from the client.
 * - Attempts to **retrieve a cached response** for the requested resource.
 * - If the cached response is **valid**, sends it to the client.
 * - If the cached response **requires revalidation**, sends a conditional request (`If-None-Match`, `If-Modified-Since`).
 * - If no cache exists or validation fails, forwards the request to the origin server.
 * - Handles different response types:
 *   - **Chunked transfer encoding**
 *   - **Large responses (`Content-Length > 65536`)**
 *   - **Regular responses**
 * - If the response is `200 OK`, stores it in the cache.
 * - Catches exceptions related to server communication and logs errors.
 * If the requested content is unchanged (`304 Not Modified`), the cached response is used instead.
 *
 * @param client_fd The client socket file descriptor.
 * @param request The parsed `Request` object.
 * @param request_id The unique request identifier for logging.
 */
void Proxy::processGet(int client_fd, Request& request, int request_id){
    // Get the request info from the parsed Request object
    string host = request.host;
    string url = request.url;
    string full_url = host + url;

    CacheStatus cache_result;
    // Get response from cache first
    Response* cached_resp = cache.get(full_url, cache_result);

    if(cached_resp != NULL){
        // When a cached response is got, log the response received from the cache
        logger->log_cache_request(request_id, cache_result, cached_resp->getExpireTime());
    } else{
        logger->log_cache_request(request_id, CacheStatus::NOT_IN_CACHE, "");
    }
    
    // When valid cache response is get
    if(cache_result == CacheStatus::VALID){
        string response_str = cached_resp->toString();
        send(client_fd, response_str.c_str(), response_str.length(), 0);

        std::string status_line = "HTTP/1.1 " + std::to_string(cached_resp->getStatusCode()) + " " + cached_resp->getStatusMessage();
        if (!status_line.empty()) {
            status_line.erase(status_line.find_last_not_of("\r\n ") + 1); // Delete the last change line char from status message
        }
        logger->log_responding(request_id, status_line); // Log response from the cache
        return;
    } 
    // When a revalidation for the cache is required
    else if(cache_result == CacheStatus::REQUIRES_VALIDATION){ 
        int port = 80; // default port
        if(request.port != ""){
            try{
                port = stoi(request.port);
            } catch (...){ // try to catch if there is any exception from conver the string port to int
                port = 80;
            }
        }

        int server_fd = connectServer(host, port); // Create a new connection for revalidation
        if(server_fd < 0){
            logger->log_error(request_id, "Failed to connect to server for validation");
            sendErrorResponse(client_fd, 502, "Bad Gateway");
            return;
        }

        Request validation_request = request;
        if (!cached_resp->getETag().empty()){
            validation_request.IfNoneMatch = cached_resp->getETag(); // Put the response Etage to the request for validaion
            logger->log_note(request_id, "Using ETag for validation: " + cached_resp->getETag());
        }

        if (!cached_resp->getLastModified().empty()){
            validation_request.IfModifiedSince = cached_resp->getLastModified(); // Put response last modified time to request
            logger->log_note(request_id, "Using Last-Modified for validation: " + cached_resp->getLastModified());
        }

        if (cached_resp->getETag().empty() && cached_resp->getLastModified().empty()) {
            logger->log_note(request_id, "Validation not possible - no validator headers");
            close(server_fd);
        } else{
            // send revalidation request
            string tranformed_request = validation_request.Request_line(); // Processed new request line with all revalidation properties added on
            logger->log_requesting(request_id, tranformed_request, host); // log the request to the origin server
            send(server_fd, tranformed_request.c_str(), tranformed_request.length(), 0);

            string validation_resp_str;
            try{
                validation_resp_str = receiveFromSocket(server_fd); // get a new response from server

                if(validation_resp_str.empty()){
                    logger->log_error(request_id, "Empty validation response from server");
                    close(server_fd);
                } else{
                    Response* validation_resp = new Response();
                    try{
                        validation_resp->parseResponse(validation_resp_str);

                        std::string status_line = "HTTP/1.1 " + to_string(validation_resp->getStatusCode()) + " " + validation_resp->getStatusMessage();
                        if (!status_line.empty()) {
                            status_line.erase(status_line.find_last_not_of("\r\n ") + 1);
                        }
                        logger->log_received(request_id, status_line, host); // log response from server
                        
                        // if 304 not modified received, use cached response
                        if(validation_resp->getStatusCode() == 304){
                            logger->log_note(request_id, "Validation successful - using cached copy");
                            string resp_str = cached_resp->toString(); // use cached response
                            send(client_fd, resp_str.c_str(), resp_str.length(), 0);
                            status_line = "HTTP/1.1 " + std::to_string(cached_resp->getStatusCode()) + " " + cached_resp->getStatusMessage();
                            if (!status_line.empty()) {
                                status_line.erase(status_line.find_last_not_of("\r\n ") + 1);
                            }
                            logger->log_responding(request_id, status_line);
                            delete validation_resp;
                            close(server_fd);
                            return;
                        } else{
                            // Content modifed, get response from original server
                            logger->log_note(request_id, "Content changed - using new response");
                            delete validation_resp;
                        }
                    } catch (...){ // catch all exceptions received from parse validation request
                        logger->log_error(request_id, "Failed to parse validation response");
                        delete validation_resp;
                    }
                }
            } catch(...){
                    logger->log_error(request_id, "Error receiving validation response");
            }
        }
    }

    // need to fetch response from origin server
    int port = 80;
    if(request.port != ""){ // get client port
        try{
            port = stoi(request.port);
        } catch(...){
            port = 80;
        }
    }   

    logger->log_requesting(request_id, request.requestHeader, host);

    int server_fd = connectServer(host, port);
    if (server_fd < 0) {
        sendErrorResponse(client_fd, 502, "Bad Gateway");
        return;
    }

    // Send request to server
    std::string transformed_request = request.Request_line();
    send(server_fd, transformed_request.c_str(), transformed_request.length(), 0);

    Response* server_response = new Response();
    try{
        // only get initial header to check if response is chunked or too long
        string inital_resp = receiveFromSocket(server_fd, 5); 

        if(inital_resp.empty()){
            logger->log_error(request_id, "Empty response from server");
            close(server_fd);
            delete server_response;
            sendErrorResponse(client_fd, 502, "Bad Gateway");
            return;
        }
        server_response->parseResponse(inital_resp);
        if(server_response->getIsChunked()){
            logger->log_note(request_id, "Detected chunked encoding");
                
            // Send the initial part to client
            send(client_fd, inital_resp.c_str(), inital_resp.length(), 0);
            vector<char> chunked_data = handleChunkResponse(server_fd, client_fd);
            server_response->addChunkedData(chunked_data);
        } else if(server_response->getContentLength() > 65536){
            logger->log_note(request_id, "Detected large content: " + 
            std::to_string(server_response->getContentLength()) + " bytes");

            vector<char> long_data = handleLongResponse(server_fd);
            server_response->addResponseBody(string(long_data.begin(), long_data.end()));

            string resp_str = server_response->toString();
            send(client_fd, resp_str.c_str(), resp_str.length(), 0);
        } else{ // Regular response
            if (server_response->getContentLength() > 0) {
                std::string rest_of_response = receiveFromSocket(server_fd);
                server_response->addResponseBody(rest_of_response);
            }

            string resp_str = server_response->toString();
            send(client_fd, resp_str.c_str(), resp_str.length(), 0);
        }

        // Log the response from server
        std::string status_line = "HTTP/1.1 " + std::to_string(server_response->getStatusCode()) + " " + server_response->getStatusMessage();
        if (!status_line.empty()) {
            status_line.erase(status_line.find_last_not_of("\r\n ") + 1);
        }
        logger->log_received(request_id, status_line, host);
        // Log response details
        if (!server_response->getETag().empty()) {
            logger->log_note(request_id, "ETag: " + server_response->getETag());
        }
            
        if (!server_response->getCacheControl().empty()) {
            logger->log_note(request_id, "Cache-Control: " + server_response->getCacheControl());
        }

        if(server_response->getStatusCode() == 200){
            handleCaching(server_response, full_url, request_id); // If 200 ok is received, cache response 
        } else{
            logger->log_responding(request_id, status_line);
            delete server_response;
        }
    } catch(const exception& e){
        // Catch exceotions, log the error, delete created response from server, and send error message
        close(server_fd);
        logger->log_error(request_id, std::string("Failed to process server response: ") + e.what());
        delete server_response;
        sendErrorResponse(client_fd, 502, "Exception detected for GET response from server");
    }
    close(server_fd);
    return;
}

/**
 * Processes an HTTP POST request from the client.
 * - Determines the host and port from the request.
 * - Forwards the request to the origin server.
 * - Receives and processes the response:
 *   - If `Transfer-Encoding: chunked`, reads chunked response.
 *   - If `Content-Length` is present, ensures the full response body is received.
 * - Sends the complete response to the client.
 * - Logs request forwarding, server response, and any errors encountered.
 *
 * If an error occurs at any stage, a `502 Bad Gateway` error is sent to the client.
 *
 * @param client_fd The socket file descriptor for the client.
 * @param request The parsed `Request` object containing the HTTP request details.
 * @param request_id The unique identifier for this request.
 */
void Proxy::processPost(int client_fd, Request& request, int request_id) {
    string host = request.host;
    int port = 80;
    if(request.port != "") {
        try {
            port = stoi(request.port);
        } catch(...) {
            port = 80;
        }
    }

    logger->log_requesting(request_id, request.requestHeader, host);

    int server_fd = connectServer(host, port);
    if(server_fd < 0) {
        sendErrorResponse(client_fd, 502, "Unable connect to server");
        return;
    }

    string request_str = request.Request_line();
    send(server_fd, request_str.c_str(), request_str.length(), 0);

    Response* server_resp = new Response();
    try {
        // Get initial response headers
        string initial_resp = receiveFromSocket(server_fd, 5);
        
        if(initial_resp.empty()) {
            logger->log_error(request_id, "Empty response from server");
            close(server_fd);
            delete server_resp;
            sendErrorResponse(client_fd, 502, "Bad Response: from POST server");
            return;
        }

        server_resp->parseResponse(initial_resp);
        
        if(server_resp->getIsChunked()) {
            logger->log_note(request_id, "Detected chunked encoding");
            
            // Send the initial part to client
            send(client_fd, initial_resp.c_str(), initial_resp.length(), 0);
            vector<char> chunked_data = handleChunkResponse(server_fd, client_fd);
            server_resp->addChunkedData(chunked_data);
        } else if(server_resp->getContentLength() > 0) {
            // For regular responses with a body
            if (server_resp->getBody().length() < static_cast<size_t>(server_resp->getContentLength())) {
                // If we need more data, use handleLongResponse
                logger->log_note(request_id, "Getting remaining body data");
                vector<char> additional_data = handleLongResponse(server_fd);
                server_resp->addResponseBody(string(additional_data.begin(), additional_data.end()));
            }
            
            // Send the complete response to the client
            string resp_str = server_resp->toString();
            send(client_fd, resp_str.c_str(), resp_str.length(), 0);
        } else {
            // No body or already complete
            string resp_str = server_resp->toString();
            send(client_fd, resp_str.c_str(), resp_str.length(), 0);
        }

        std::string status_line = "HTTP/1.1 " + std::to_string(server_resp->getStatusCode()) + " " + 
                                  server_resp->getStatusMessage();
        status_line.erase(status_line.find_last_not_of("\r\n ") + 1);
        logger->log_received(request_id, status_line, host);
        logger->log_responding(request_id, status_line);

    } catch(const exception& e) { // Try to catch the exceptions for the whole POST process
        close(server_fd);
        logger->log_error(request_id, std::string("Failed to process server response: ") + e.what());
        delete server_resp;
        sendErrorResponse(client_fd, 502, "Bad Gateway");
        return;
    }
    
    // Clean up
    close(server_fd);
    delete server_resp;
}

/**
 * Handles an HTTP CONNECT request for establishing an HTTPS tunnel.
 * - Establishes a TCP connection to the target server.
 * - Sends `HTTP/1.1 200 Connection established` to the client.
 * - Uses `select()` for bidirectional communication between client and server.
 * - Transfers data until one side closes the connection or an error occurs.
 * - Logs tunnel creation, data transfer issues, and tunnel closure.
 *
 * @param client_fd The socket file descriptor for the client.
 * @param request The parsed `Request` object containing the CONNECT request details.
 * @param request_id The unique identifier for this request.
 */
void Proxy::processConnect(int client_fd, Request& request, int request_id){
    string host = request.host;
    int port = 443;
    if(request.port != ""){
        try{
            port = stoi(request.port);
        } catch(...){
            port = 443;
        }
    }

    int server_fd = connectServer(host, port);
    if(server_fd < 0){
        logger->log_error(request_id, "Failed to connect to server for connect");
        sendErrorResponse(client_fd, 502, "Bad Gateway");
        return;
    }

    string response = "HTTP/1.1 200 Connection established\r\n\r\n";
    send(client_fd, response.c_str(), response.length(), 0);

    logger->log_responding(request_id, "HTTP/1.1 200 Connection established");

    int max_fd = max(client_fd, server_fd) + 1;
    fd_set readfds;
    struct timeval tv;
    int fd[2] = {server_fd, client_fd}; // Keep listening to both client and server
    char buffer[65536];
    bool tunnel_active = true;

    while(running && tunnel_active){
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        FD_SET(client_fd, &readfds);

        tv.tv_sec = 10;
        tv.tv_usec = 500000;

        int rv = select(max_fd, &readfds, NULL, NULL, &tv);
        if(rv == 0){
            logger->log_note(request_id, "Tunnel timeout after 10.5 seconds of inactivity");
            tunnel_active = false;
            break;
        }

        if(rv == -1){
            logger->log_error(request_id, "Select error in tunnel");
            tunnel_active = false;
            break;
        }

        for(int i = 0; i < 2; i++){
            if(FD_ISSET(fd[i], &readfds)){
                memset(buffer, 0, sizeof(buffer));
                int byte_received = recv(fd[i], buffer, sizeof(buffer), MSG_NOSIGNAL);
                
                string receive_from = (i == 0) ? "server" : "client";
                if(byte_received <= 0){
                    logger->log_note(request_id, "Connection closed by " + receive_from);
                    tunnel_active = false;
                    break;
                }

                string sendTo = (i == 0) ? "client" : "server";
                int byte_sent = send(fd[1-i], buffer, byte_received, MSG_NOSIGNAL); // send the received contents to the other direction
                if(byte_sent <= 0){
                    logger->log_error(request_id, "Failed to forward data to " + sendTo);
                    tunnel_active = false;
                    break;
                }
            }
        }
    }

    logger->log_tunnel_closed(request_id);
    close(server_fd);
}

/**
 * Constructs the Proxy server.
 * Initialze all varibales: Specify log address; Specify cache max size to be 50
 * Finilize initial listening and binding operations for the sockets
 *
 * @param port The port number on which the proxy listens for client connections.
 * @throws `std::runtime_error` if socket creation, binding, or listening fails.
 */
Proxy::Proxy(int port) : logger(make_unique<Logger>("/var/log/erss/proxy.log")), cache(50), request_count(0), running(false) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd);
        throw std::runtime_error("Failed to set socket options");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if(::bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        close(server_fd);
        throw std::runtime_error("Failed to bind to port " + std::to_string(port)); // Throw an exception
    }

    if(listen(server_fd, 100) < 0){
        close(server_fd);
        throw std::runtime_error("Failed to listen on socket"); // Throw listen exception
    }

    logger->log_note(-1, "Proxy started on port " + std::to_string(port));
}

/**
 * Handles a client request by spawning a new thread.
 * Eexecuted in a separate thread for each client request.
 *
 * @param client_fd The socket file descriptor for the client.
 * @param client_addr The `sockaddr_in` structure containing the client's address.
 */
void Proxy::handleClientRequest(int client_fd, sockaddr_in client_addr) {
    receiveClient(client_fd, client_addr);
    close(client_fd);
}

/**
 * Runs the Proxy server, accepting and handling client connections.
 * - Enters a loop to accept incoming client connections.
 * - Spawns a new thread for each accepted connection.
 * - Uses `select()` with a timeout to check for incoming connections.
 * - Logs errors for failed connections and thread creation issues.
 * - Maintains a list of active threads and removes finished ones.
 */
void Proxy::run() {
    if (!running) {
        running = true;
        logger->log_note(-1, "Proxy started and waiting for connections");
    }
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;  // Check running flag every second
        tv.tv_usec = 0;
        
        int ready = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        
        // Check if running is finished
        if (!running) {
            break;
        }
        
        if (ready <= 0) {
            continue; // Does not listen to any request
        }
        
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (running) {
                logger->log_error(-1, "Failed to accept connection");
            }
            continue;
        }
        
        struct timeval tv_client;
        tv_client.tv_sec = 30;
        tv_client.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_client, sizeof(tv_client));
        
        try {
            std::lock_guard<std::mutex> lock(requested_mutex);
            
            // Update the threads
            for (auto it = threads.begin(); it != threads.end();) {
                if (it->joinable()) {
                    it->join();
                    it = threads.erase(it);
                } else {
                    it++;
                }
            }
            
            // Create new thread for this request and call the functions to handle the request
            threads.emplace_back(&Proxy::handleClientRequest, this, client_fd, client_addr);
            
            threads.back().detach();
            
            logger->log_note(-1, "Spawned new thread for client connection. Active threads: " + 
                           std::to_string(threads.size()));
        }
        catch (const std::exception& e) { // Catch exceptions
            logger->log_error(-1, "Failed to create thread: " + std::string(e.what()));
            close(client_fd);
        }
    }
}

/**
 * When closing down signal is received, close all threads and shut down the connection to stop the proxy.
 * - Sets the `running` flag to `false`.
 * - Closes the server socket to prevent new connections.
 * - Waits for all active threads to finish execution.
 * - Logs server shutdown and terminates gracefully.
 */
void Proxy::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd);
    
    lock_guard<mutex> lock(requested_mutex);
    
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads.clear();
    logger->log_note(-1, "Proxy stopped");
    std::cout << "All threads terminated, proxy stopped successfully" << std::endl;
    return;
}
