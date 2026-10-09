#ifndef SERVER_PARSER_H
#define SERVER_PARSER_H

#include <string>
#include <cstdint>
#include <vector>

struct parsed_server_data {
    std::string address;            // IPv4 address or server domain name.
    uint16_t port;                  // port number (0 - 65535). Zero stands for default port. 
    uint8_t server_timeout;         // 1 - 99 seconds.
    std::vector<uint8_t> pawn_row;  // row of 0/1 values indicating the presence of the pawns (0 - absent, 1 - present).
    uint8_t max_pawn;               // max number of pawns in a row (length of pawn_row -1) 0-indexed
};

// Parses command line arguments for server and validates them. If the arguments are invalid, the function will exit with an error message.
parsed_server_data parse_server_data(int argc, char* argv[]);


#endif // SERVER_PARSER_H