#ifndef _PROXY_HPP_
#define _PROXY_HPP_

#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <atomic>
#include <memory>
#include <functional>
#include <condition_variable>
#include <cstring>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include "cache.hpp"
#include "log.hpp"
#include "request.hpp"
#include "response.hpp"
#include "util.hpp"

using namespace std;

class Proxy{
private:
    int server_fd;
    unique_ptr<Logger> logger;
    Cache cache;
    atomic<int> request_count;
    atomic<bool> running;
    vector<thread> threads;
    mutex requested_mutex;

    int generateRequestID();
    string receiveFromSocket(int socket_fd, int timeout = 10);
    vector<char> handleChunkResponse(int server_fd, int client_fd);
    vector<char> handleLongResponse(int server_fd);
    void handleCaching(Response* response, const string& url, int request_id);
    void receiveClient(int client_fd, struct sockaddr_in client_addr);
    void sendErrorResponse(int client_fd, int status_code, const string& reason);
    int connectServer(const string& host, int port);
    void processGet(int client_fd, Request& request, int request_id);
    void processPost(int client_fd, Request& request, int request_id);
    void processConnect(int client_fd, Request& request, int request_id);
    void handleClientRequest(int client_fd, sockaddr_in client_addr);

public:
    Proxy(int port);
    void run();
    void stop();

};

#endif