#include <netdb.h>
#include <cstring>
#include <string>

#include "err.h"
#include "messages.h"
#include "common.h"

sockaddr_in get_address(const std::string& host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP
    hints.ai_protocol = IPPROTO_UDP; // UDP

    struct addrinfo *address_result;
    
    std::string port_str = std::to_string(port);

    int errcode = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo failed: " + std::string(gai_strerror(errcode)));
    }

    struct sockaddr_in send_address = *(struct sockaddr_in*)(address_result->ai_addr);
    
    freeaddrinfo(address_result);

    return send_address;
}
