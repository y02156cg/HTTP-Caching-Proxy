#include "proxy.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

// Global signal handling
Proxy* global_proxy = NULL;

/**
 * Handle the given signal.
 * When `sig` is received, stops the proxy.
 */
void signalHandler(int sig) {
    cout << "Received termination signal. Shutting down..." << endl;
    global_proxy->stop();
}

/**
 * Entry point for the HTTP proxy server.
 *
 * usage:  `./proxy <port>`
 *
 * The function:
 * - Reads the port number from command-line arguments.
 * - Initializes and starts the `Proxy` server.
 * - Handles termination signals (`SIGINT`) for a clean shutdown.
 * - Catches and reports exceptions related to server initialization or runtime errors.
 */
int main(int argc, char* argv[]) {
    if(argc != 2){
        std::cerr << "Port number should be included in arguments" << endl;
        return 1;
    }
    
    int port = stoi(argv[1]);

    try {
        Proxy proxy(port);
        global_proxy = &proxy;
        signal(SIGINT, signalHandler);
        
        std::cout << "Proxy started. Press Ctrl+C to stop." << std::endl;
        proxy.run();
        
    } catch (const std::exception& e) { // If exception received, send error and stop the whole program using return 
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}