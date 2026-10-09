#ifndef CLIENT_PARSER_H
#define CLIENT_PARSER_H

#include "messages.h" 
#include <string>
#include <cstdint>


// Structure with validated and parsed input data for the client.
struct parsed_client_data {
    std::string address;        // validated IPv4 address or server domain.
    uint16_t port;              // validated server port number (from 1 to 2^16 - 1).
    uint8_t  server_timeout;    // validated number of timeout seconds (1 - 99). 
    client_msg message_struct;  // validated struct with message for the server.
};


// Parses command line arguments for client
parsed_client_data parse_input_client(int argc, char* argv[]);

#endif // CLIENT_PARSER_H