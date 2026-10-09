#ifndef COMMON_H
#define COMMON_H
#include <netdb.h>
#include <cstring>
#include <string>

#include "messages.h" 

// Function to resolve a hostname and port to a socket address.
sockaddr_in get_address(const std::string& host, uint16_t port);

#endif // COMMON_H