#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include <cstdint>
#include "messages.h"
#include "game_session.h"
#include <map>
#include <vector>
#include <netinet/in.h>
#include <chrono>
#include <cstdlib>

// Checks if the pawn at the given index is standing (1) or not (0).
bool is_pawn_standing(game_state* s, uint8_t idx);
// Knocks down the pawn at the given index (sets pawn value to 0).
void knock_down_pawn(game_state* s, uint8_t idx);
// Checks if all pawns on the board are knocked down (are equal to 0).
bool is_board_empty(game_state* s);

// Sends a MSG_WRONG_MSG response to the client with details about
// the error.
void send_wrong_msg(
	int socket_fd,
	const sockaddr_in& target_addr,
	const uint8_t* original_msg,
	size_t original_msg_length,
	uint8_t error_index
);
// Sends the current game state to the player.
void send_state_to_player(
	int socket_fd,
	const game_session& session,
	const sockaddr_in& target_addr
);

// Handles a client's request to join a game.
void handle_join(
	int socket_fd,
	const msg_join* msg,
	const sockaddr_in& client_addr,
	uint8_t max_pawn,
	const std::vector<uint8_t>& pawn_row,
	std::map<uint32_t, game_session>& active_games,
	uint32_t& next_game_id,
	uint32_t& waiting_game_id
);
// Handles a client's move (MSG_MOVE_1 or MSG_MOVE_2) and updates
// the game state accordingly.
void handle_move(
	int socket_fd,
	const msg_move* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
);
// Handles a client's request to give up the game and updates
// the game state accordingly.
void handle_giving_up(
	int socket_fd,
	const msg_game_action* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
);
// Handles a client's keep-alive message.
void handle_keep_alive(
	int socket_fd,
	const msg_game_action* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
);

// Cleans up inactive game sessions based on the server timeout value.
void cleanup_inactive_games(
	uint8_t server_timeout,
	std::map<uint32_t, game_session>& active_games
);

#endif // SERVER_HANDLERS_H