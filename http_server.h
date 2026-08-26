#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "sql_database.h"

class HTTPServer {
private:
    int port;
    SOCKET listenSocket;
    bool running;
    SQLDatabase& db;

    std::string processRequest(const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers, const std::string& body);
    std::string getContentType(const std::string& path);
    std::string serveStaticFile(const std::string& filepath);

public:
    HTTPServer(int port, SQLDatabase& database);
    ~HTTPServer();

    bool start();
    void stop();
    void runServerLoop();
    void handleClient(SOCKET clientSocket);
};

#endif
