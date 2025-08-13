

#include "../include/RedisServer.h"
#include "../include/RedisCommandHandler.h"
#include "../include/RedisDatabase.h"

#include <iostream>
#include <vector>
#include <thread>
#include <cstring>

// For Windows Sockets
#include <winsock2.h>
#include <ws2tcpip.h>

// For signal handling on Windows
#include <windows.h>

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

static RedisServer* globalServer = nullptr;

BOOL WINAPI consoleHandler(DWORD dwType) {
    if (dwType == CTRL_C_EVENT) {
        if (globalServer) {
            std::cout << "\nCaught signal CTRL-C, shutting down...\n";
            globalServer->shutdown();
        }
        return TRUE;
    }
    return FALSE;
}

void RedisServer::setupSignalHandler() {
    SetConsoleCtrlHandler(consoleHandler, TRUE);
}

RedisServer::RedisServer(int port) : port(port), server_socket(INVALID_SOCKET), running(true) {
    globalServer = this;
    setupSignalHandler();
}

void RedisServer::shutdown() {
    running = false;
    if (server_socket != INVALID_SOCKET) {
        if (RedisDatabase::getInstance().dump("dump.my_rdb"))
            std::cout << "Database Dumped to dump.my_rdb\n";
        else
            std::cerr << "Error dumping database\n";
        closesocket(server_socket);
        WSACleanup();
    }
    std::cout << "Server Shutdown Complete!\n";
}

void RedisServer::run() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Error Creating Server Socket: " << WSAGetLastError() << "\n";
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(server_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Error Binding Server Socket: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, 10) == SOCKET_ERROR) {
        std::cerr << "Error Listening On Server Socket: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    std::cout << "Redis Server Listening On Port " << port << "\n";

    std::vector<std::thread> threads;
    RedisCommandHandler cmdHandler;

    while (running) {
        SOCKET client_socket = accept(server_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            if (running)
                std::cerr << "Error Accepting Client Connection: " << WSAGetLastError() << "\n";
            break;
        }

        threads.emplace_back([client_socket, &cmdHandler]() {
            char buffer[1024];
            while (true) {
                memset(buffer, 0, sizeof(buffer));
                int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) break;
                std::string request(buffer, bytes);
                std::string response = cmdHandler.processCommand(request);
                send(client_socket, response.c_str(), response.size(), 0);
            }
            closesocket(client_socket);
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    if (RedisDatabase::getInstance().dump("dump.my_rdb"))
        std::cout << "Database Dumped to dump.my_rdb\n";
    else
        std::cerr << "Error dumping database\n";
}