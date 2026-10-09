#include <vector>
#include <map>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstddef>
#include <cerrno>

#include "err.h"
#include "server_parser.h"
#include "server_handlers.h"
#include "game_session.h"

// --- Helper functions for pawn operations ---

bool is_pawn_standing(game_state* s, uint8_t idx)
{
	if (idx > s->max_pawn) return false;
	return (s->pawn_row[idx / 8] & (1 << (7 - (idx % 8)))) != 0;
}

void knock_down_pawn(game_state* s, uint8_t idx)
{
	if (idx <= s->max_pawn)
		s->pawn_row[idx / 8] &= ~(1 << (7 - (idx % 8)));
}

bool is_board_empty(game_state* s)
{
	for (int i = 0; i <= s->max_pawn; ++i)
	{
		if (is_pawn_standing(s, i)) return false;
	}
	return true;
}

// --- Helper functions for sending messages to clients ---

void send_wrong_msg(
	int socket_fd,
	const sockaddr_in& target_addr,
	const uint8_t* original_msg,
	size_t original_msg_length,
	uint8_t error_index
)
{
	wrong_msg msg;
	std::memset(&(msg.original_msg), 0, sizeof(msg.original_msg));

	size_t length_to_copy = std::min(
		static_cast<size_t>(sizeof(msg.original_msg)),
		original_msg_length
	);
	std::memcpy(&(msg.original_msg), original_msg, length_to_copy);

	// There's no need to do htonl() because fields below are 1 byte only.
	msg.status = MSG_WRONG_MSG;
	msg.error_index = error_index;

	ssize_t sent_length = sendto(
		socket_fd,
		&msg,
		sizeof(msg),
		0,
		reinterpret_cast<const sockaddr*>(&target_addr),
		sizeof(target_addr)
	);

	// Sending failed. Log the error and return.
	if (sent_length < 0)
	{
		warn("Failed to send MSG_WRONG_MSG to client. Error: "
			+ std::string(std::strerror(errno)));
	}
	else if (sent_length != sizeof(msg))
	{
		warn("Incomplete sendto MSG_WRONG_MSG. Error: "
			+ std::string(std::strerror(errno)));
	}
}

void send_state_to_player(
	int socket_fd,
	const game_session& session,
	const sockaddr_in& target_addr
)
{
	if (!session.state) return;

	ssize_t sent = sendto(
		socket_fd,
		session.state,
		session.state_size,
		0,
		reinterpret_cast<const sockaddr*>(&target_addr),
		sizeof(target_addr)
	);
	if (sent < 0)
	{
		warn("Failed to send state to player. Error: "
			+ std::string(std::strerror(errno)));
	}
}

// --- Helper functions for handling client messages ---

void handle_move(
	int socket_fd,
	const msg_move* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
)
{
	uint32_t req_game_id = ntohl(msg->game_id);
	uint32_t req_player_id = ntohl(msg->player_id);
	uint8_t target_pawn = msg->pawn;

	if (req_player_id == 0)
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_move),
			offsetof(msg_move, player_id)
		);
		return;
	}

	auto it = active_games.find(req_game_id);
	if (it == active_games.end())
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_move),
			offsetof(msg_move, game_id)
		);
		return;
	}

	game_session& session = it->second;
	game_state* s = session.state;

	// Check if the player is part of this game session.
	bool is_player_a = (req_player_id == ntohl(s->player_a_id));
	bool is_player_b = (req_player_id == ntohl(s->player_b_id));

	if (!is_player_a && !is_player_b)
	{
		// Player is not part of this game session, but since server
		// could interpret the player_id (because it wasn't 0),
		// the earliest byte that server couldn't interpret is game_id.
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_move),
			offsetof(msg_move, game_id)
		);
		return;
	}

	if (s->status == WIN_A || s->status == WIN_B)
		session.game_end_time = std::chrono::steady_clock::now();

	if (is_player_a)
		session.last_activity_a = std::chrono::steady_clock::now();
	if (is_player_b)
		session.last_activity_b = std::chrono::steady_clock::now();

	bool is_valid_turn =
		(is_player_a && s->status == TURN_A)
		|| (is_player_b && s->status == TURN_B);
	if (!is_valid_turn)
	{
		send_state_to_player(socket_fd, session, client_addr);
		return;
	}

	bool move_valid = false;
	if (msg->msg_type == MSG_MOVE_1)
	{
		if (is_pawn_standing(s, target_pawn))
		{
			knock_down_pawn(s, target_pawn);
			move_valid = true;
		}
	}
	else if (msg->msg_type == MSG_MOVE_2)
	{
		// Preventing the edge case of trying to knock down two pawns
		// starting from the last pawn.
		if (
			target_pawn < s->max_pawn
			&& is_pawn_standing(s, target_pawn)
			&& is_pawn_standing(s, target_pawn + 1)
		)
		{
			knock_down_pawn(s, target_pawn);
			knock_down_pawn(s, target_pawn + 1);
			move_valid = true;
		}
	}

	if (!move_valid)
	{
		// Invalid move is still correct and accepted, but doesn't
		// change the game state.
		send_state_to_player(socket_fd, session, client_addr);
		return;
	}

	// End of the game.
	if (is_board_empty(s))
	{
		s->status = (s->status == TURN_A) ? WIN_A : WIN_B;
		session.game_end_time = std::chrono::steady_clock::now();
	}
	else
	{
		s->status = (s->status == TURN_A) ? TURN_B : TURN_A;
	}

	send_state_to_player(socket_fd, session, client_addr);
}

void handle_join(
	int socket_fd,
	const msg_join* msg,
	const sockaddr_in& client_addr,
	uint8_t max_pawn,
	const std::vector<uint8_t>& pawn_row,
	std::map<uint32_t, game_session>& active_games,
	uint32_t& next_game_id,
	uint32_t& waiting_game_id
)
{
	uint32_t client_player_id = ntohl(msg->player_id);

	if (client_player_id == 0)
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_join),
			offsetof(msg_join, player_id)
		);
		return;
	}

	if(waiting_game_id != 0)
	{
		// Player joins as player B to the waiting game session.
		auto it = active_games.find(waiting_game_id);
        if (it != active_games.end() &&
				 		it->second.state->status == WAITING_FOR_OPPONENT)
        {
            game_session& session = it->second;
            session.has_player_b = true;
            session.state->player_b_id = htonl(client_player_id);
            session.state->status = TURN_B;

            if (session.state->player_b_id == session.state->player_a_id)
            {
                session.last_activity_a = std::chrono::steady_clock::now();
            }
            session.last_activity_b = std::chrono::steady_clock::now();

            send_state_to_player(socket_fd, session, client_addr);
            waiting_game_id = 0; // Game is full, reset waiting_game_id.
            return;
        }
        else
        {
            waiting_game_id = 0;
        }
	}

	if (next_game_id == 0)
	{
		warn("Game ID overflow! Cannot create a new game session.");
		return;
	}

	try
	{
		game_session& new_session = active_games[next_game_id];

		size_t board_bytes = (max_pawn / 8) + 1;
		size_t total_size = sizeof(game_state) + board_bytes;

		new_session.state = static_cast<game_state*>(std::malloc(total_size));
		if (!new_session.state)
		{
			syswarn("Failed to allocate memory for new game session state.");
			// Ignore this join request, erase previously added session
			// with uninitialized state.
			active_games.erase(next_game_id);
			return;
		}

		new_session.state_size = total_size;

		// Initialize the game state for the new session.
		game_state* s = new_session.state;
		s->game_id = htonl(next_game_id);
		s->player_a_id = htonl(client_player_id);
		s->player_b_id = 0;
		s->status = WAITING_FOR_OPPONENT;
		s->max_pawn = max_pawn;

		// Zero the s->pawn_row, then copy the pawn_row bytes from the
		// input vector.
		std::memset(s->pawn_row, 0x00, board_bytes);
		size_t bytes_to_copy = std::min(board_bytes, pawn_row.size());
		std::memcpy(s->pawn_row, pawn_row.data(), bytes_to_copy);

		new_session.last_activity_a = std::chrono::steady_clock::now();

		send_state_to_player(socket_fd, new_session, client_addr);

		waiting_game_id = next_game_id;
		next_game_id++;
	}
	catch(const std::bad_alloc& e){
		syswarn("std::bad_alloc caught");
	}

}

void handle_giving_up(
	int socket_fd,
	const msg_game_action* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
)
{
	uint32_t req_game_id = ntohl(msg->game_id);
	uint32_t req_player_id = ntohl(msg->player_id);

	if (req_player_id == 0)
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, player_id)
		);
		return;
	}

	auto it = active_games.find(req_game_id);
	if (it == active_games.end())
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, game_id)
		);
		return;
	}

	game_session& session = it->second;
	game_state* s = session.state;

	bool is_player_a = (req_player_id == ntohl(s->player_a_id));
	bool is_player_b = (req_player_id == ntohl(s->player_b_id));

	if (!is_player_a && !is_player_b)
	{
		// Player is not part of this game session, but since server
		// could interpret the player_id (because it wasn't 0),
		// the earliest byte that server couldn't interpret is game_id.
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, game_id)
		);
		return;
	}

	if (s->status == WIN_A || s->status == WIN_B)
		session.game_end_time = std::chrono::steady_clock::now();

	if (is_player_a)
		session.last_activity_a = std::chrono::steady_clock::now();
	if (is_player_b)
		session.last_activity_b = std::chrono::steady_clock::now();

	bool is_valid_turn =
		(is_player_a && s->status == TURN_A)
		|| (is_player_b && s->status == TURN_B);
	if (!is_valid_turn)
	{
		send_state_to_player(socket_fd, session, client_addr);
		return;
	}

	// The winning player is the one who is not giving up.
	// This also applies to a case of a player giving up while playing
	// against themselves.
	s->status = (s->status == TURN_A) ? WIN_B : WIN_A;
	session.game_end_time = std::chrono::steady_clock::now();

	send_state_to_player(socket_fd, session, client_addr);
}

void handle_keep_alive(
	int socket_fd,
	const msg_game_action* msg,
	const sockaddr_in& client_addr,
	std::map<uint32_t, game_session>& active_games
)
{
	uint32_t req_game_id = ntohl(msg->game_id);
	uint32_t req_player_id = ntohl(msg->player_id);

	if (req_player_id == 0)
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, player_id)
		);
		return;
	}

	auto it = active_games.find(req_game_id);
	if (it == active_games.end())
	{
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, game_id)
		);
		return;
	}

	game_session& session = it->second;
	game_state* s = session.state;

	bool is_player_a = (req_player_id == ntohl(s->player_a_id));
	bool is_player_b = (req_player_id == ntohl(s->player_b_id));

	if (!is_player_a && !is_player_b)
	{
		// Player is not part of this game session, but since server
		// could interpret the player_id (because it wasn't 0),
		// the earliest byte that server couldn't interpret is game_id.
		send_wrong_msg(
			socket_fd,
			client_addr,
			reinterpret_cast<const uint8_t*>(msg),
			sizeof(msg_game_action),
			offsetof(msg_game_action, game_id)
		);
		return;
	}

	if (s->status == WIN_A || s->status == WIN_B)
		session.game_end_time = std::chrono::steady_clock::now();

	if (is_player_a)
		session.last_activity_a = std::chrono::steady_clock::now();
	// If player A is the same as player B, both last_activity_a and
	// last_activity_b will be updated.
	if (is_player_b)
		session.last_activity_b = std::chrono::steady_clock::now();

	send_state_to_player(socket_fd, session, client_addr);
}

// --- Helper function for cleaning up finished games ---

void cleanup_inactive_games(
	uint8_t server_timeout,
	std::map<uint32_t, game_session>& active_games
)
{
	auto now = std::chrono::steady_clock::now();

	// games_to_delete will store the IDs of the games that need to be
	// deleted after the loop.
	std::vector<uint32_t> games_to_delete;

	for (auto& pair : active_games)
	{
		uint32_t game_id = pair.first;
		game_session& session = pair.second;
		game_state* s = session.state;

		if (s->status == WAITING_FOR_OPPONENT)
		{
			// If the game is waiting for an opponent, check only A's
			// activity, because B hasn't joined yet.
			auto time_since_a =
				std::chrono::duration_cast<std::chrono::seconds>(
					now - session.last_activity_a
				).count();
			if (time_since_a >= server_timeout)
				games_to_delete.push_back(game_id);
			continue;
		}

		if (s->status == TURN_A || s->status == TURN_B)
		{
			auto timeout_limit = std::chrono::seconds(server_timeout);
			auto deadline_a = session.last_activity_a + timeout_limit;
			auto deadline_b = session.last_activity_b + timeout_limit;
			bool a_timed_out = (now >= deadline_a);
			bool b_timed_out = (now >= deadline_b);

			if (a_timed_out && b_timed_out)
			{
				// Both players timed out. Who timed out first wins.
				if (deadline_a < deadline_b)
					s->status = WIN_B;
				else
					s->status = WIN_A;
				session.game_end_time = now;
			}
			else if (a_timed_out)
			{
				s->status = WIN_B;
				session.game_end_time = now;
			}
			else if (b_timed_out)
			{
				s->status = WIN_A;
				session.game_end_time = now;
			}
			continue;
		}

		if (s->status == WIN_A || s->status == WIN_B)
		{
			auto time_since_end =
				std::chrono::duration_cast<std::chrono::seconds>(
					now - session.game_end_time
				).count();
			if (time_since_end >= server_timeout)
				games_to_delete.push_back(game_id);
			continue;
		}
	}

	for (uint32_t id : games_to_delete)
	{
		// Automatically will call the destructor of game_session
		// and free the allocated memory.
		active_games.erase(id);
	}
}
