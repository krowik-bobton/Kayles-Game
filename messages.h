#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>
#include <string>

// Enum representing the type of the message sent by the server to the clients.
enum server_msg_type : uint8_t {
    WAITING_FOR_OPPONENT = 0,  // Player A is connected, wait for player B 
    TURN_A               = 1,  // Wait for player A to make a move 
    TURN_B               = 2,  // Wait for player B to make a move
    WIN_A                = 3,  // Player A has won
    WIN_B                = 4,  // Player B has won
    MSG_WRONG_MSG        = 255 // Server received an invalid message from the client
};

// Function translating the server_msg_type enum values into strings.
inline std::string server_msg_type_name(server_msg_type type) {
    switch(type) {
        case WAITING_FOR_OPPONENT: return "WAITING_FOR_OPPONENT";
        case TURN_A:               return "TURN_A";
        case TURN_B:               return "TURN_B";
        case WIN_A:                return "WIN_A";
        case WIN_B:                return "WIN_B";
        case MSG_WRONG_MSG:        return "MSG_WRONG_MSG";
        default:                   return "UNKNOWN_MSG";
    }
}

// Structure representing the current state of the game.
// This structure is sent by the server to the clients after each correct move.
struct __attribute__((packed)) game_state {
    uint32_t        game_id;
    uint32_t        player_a_id; // Cannot be 0
    uint32_t        player_b_id; // 0 if not joined yet
    server_msg_type status;      // Game status (enum server_msg_type)
    uint8_t         max_pawn;    // Max number of pawns in a row
    uint8_t         pawn_row[];  // Array of bytes representing the pawns (Flexible Array Member)
};

// Enum representing the type of the message sent by the client to the server.
enum client_msg_type : uint8_t {
    MSG_JOIN       = 0, // Client wants to join a game
    MSG_MOVE_1     = 1, // Client wants to remove 1 pawn
    MSG_MOVE_2     = 2, // Client wants to remove 2 pawns
    MSG_KEEP_ALIVE = 3, // Client confirms that it is still active 
    MSG_GIVE_UP    = 4  // Client gives up the game
};

// Function translating the client_msg_type enum values into strings.
inline std::string client_msg_type_name(client_msg_type type) {
    switch(type) {
        case MSG_JOIN:       return "MSG_JOIN";
        case MSG_MOVE_1:     return "MSG_MOVE_1";
        case MSG_MOVE_2:     return "MSG_MOVE_2";
        case MSG_KEEP_ALIVE: return "MSG_KEEP_ALIVE";
        case MSG_GIVE_UP:    return "MSG_GIVE_UP";
        default:             return "UNKNOWN_MSG";
    }
}

// Generic structure representing the layout of a client message.
// Used for safe offset calculations.
struct __attribute__((packed)) client_msg {
    client_msg_type msg_type;  // Type of the message (enum client_msg_type)
    uint32_t        player_id; // ID of the player 
    uint32_t        game_id;   // ID of the game
    uint8_t         pawn;      // Pawn number
};

//Structure representing a MSG_JOIN message.
struct __attribute__((packed)) msg_join {
    client_msg_type msg_type;  // Always MSG_JOIN (0)
    uint32_t        player_id; // ID of the player 
};

// Structure representing a MSG_MOVE_1 or MSG_MOVE_2 message.
struct __attribute__((packed)) msg_move {
    client_msg_type msg_type;  // MSG_MOVE_1 (1) or MSG_MOVE_2 (2)
    uint32_t        player_id; // ID of the player 
    uint32_t        game_id;   // ID of the game
    uint8_t         pawn;      // Pawn number (MSG_MOVE_1) or left pawn number (MSG_MOVE_2)
};

// Structure representing a MSG_KEEP_ALIVE or MSG_GIVE_UP message.
struct __attribute__((packed)) msg_game_action {
    client_msg_type msg_type;  // MSG_KEEP_ALIVE (3) or MSG_GIVE_UP (4)
    uint32_t        player_id; // ID of the player 
    uint32_t        game_id;   // ID of the game
};

// Structure sent by the server when it receives an invalid message from the client.
struct __attribute__((packed)) wrong_msg {
    uint8_t         original_msg[12]; // Up to 12 initial bytes of the invalid client message
    server_msg_type status;           // Always MSG_WRONG_MSG (255)
    uint8_t         error_index;      // Index of the byte the server couldn't interpret (0-indexed)
};

#endif // MESSAGES_H