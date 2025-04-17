#ifndef _REQUEST_HPP_
#define _REQUEST_HPP_

#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include "util.hpp"

using namespace std;

class Request{
public:
    string httpRequest;
    string requestHeader; 
    string host;
    string userAgent;
    string url;
    string connection;
    string port;
    string method;

    string IfNoneMatch;
    string IfModifiedSince;

    Request(const string& httpRequest);

    void parseRequest();
    void setHeader(const std::string& line);
    void setHostnameAndPort(const std::string& line);

    string Request_line() const; 
};

#endif