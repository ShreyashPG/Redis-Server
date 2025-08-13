

#ifndef REDIS_COMMAND_HANDLER_H
#define REDIS_COMMAND_HANDLER_H

#include <string>

class RedisCommandHandler {
public:
    RedisCommandHandler();
    std::string processCommand(const std::string& commandLine);
};

#endif