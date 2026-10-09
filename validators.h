#ifndef VALIDATORS_H
#define VALIDATORS_H

#include "messages.h"
#include <string>
#include <vector>

// Validates the port number provided as a string and returns it as a uint16_t
// If the string is not a valid port number (not a number, out of range, etc.)
// the function will call fatal() with an error message.
// Important: function considers a port number valid
// if it's in a range (1 - UINT16_MAX)
uint16_t validate_port(const std::string &str);

// Validates the client timeout value provided 
// as a string and returns it as a uint8_t.
// If the string is not a valid timeout value 
// (not a number, out of range, etc.)
// the function will call fatal() with an error message.
uint8_t validate_timeout(const std::string &str);

// Validates and parses a client message string into a client_msg structure.
// 
// Message format: msg_type/player_id[/game_id[/pawn]]
// Fields are separated by '/' delimiter.
// 
// Field requirements depend on msg_type:
// - MSG_JOIN (0): requires 2 fields (msg_type/player_id)
// - MSG_KEEP_ALIVE (3), MSG_GIVE_UP (4): 
//      require 3 fields (msg_type/player_id/game_id)
// - MSG_MOVE_1 (1), MSG_MOVE_2 (2): require 4 fields 
//      (msg_type/player_id/game_id/pawn)
client_msg validate_client_message(const std::string &str);

// Validates an IPv4 address or domain name provided as a string.
// 
// Uses getaddrinfo() to verify that 
// the provided address is a valid IPv4 address
// or a valid domain name that can be resolved.
std::string validate_address(const std::string &str);

// Validates the pawn row string and converts it into a vector of
// bytes (uint8_t). Each character in the input string should be '0'
// or '1', where '1' indicates the presence of a pawn and '0' its
// absence. The first and last characters must be '1'.
std::vector<uint8_t> validate_pawn_row(const std::string &str);

#endif // VALIDATORS_H