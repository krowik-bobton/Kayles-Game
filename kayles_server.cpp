#include <iostream>
#include <map>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstddef>
#include <cerrno>
#include <chrono>

#include "err.h"
#include "messages.h"
#include "server_parser.h"
#include "common.h"
#include "server_handlers.h"
#include "game_session.h"

constexpr size_t BUFFER_SIZE = 512;
constexpr size_t TIME_BETWEEN_CLEANUPS_MILISECONDS = 100; // 0.1s
constexpr size_t SOCKET_TIMEOUT_MICROSECONDS = 450000; // 0.45s

int main(int argc, char* argv[])
{
	parsed_server_data config = parse_server_data(argc, argv);

	sockaddr_in server_address = get_address(config.address, config.port);

	// Map of all active game sessions, indexed by game_id.
	std::map<uint32_t, game_session> active_games;
	uint32_t next_game_id = 1;
	uint32_t waiting_game_id = 0;

	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
		syserr("cannot create a socket");

	// Put timeout of 0.45s on the socket to ensure that
	// server can regularly check for inactive games and clean them up.
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = SOCKET_TIMEOUT_MICROSECONDS;

	if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		syserr("setsockopt failed");

	if (bind(
		socket_fd,
		reinterpret_cast<sockaddr*>(&server_address),
		sizeof(server_address)
	) < 0)
	{
		syserr("bind failed");
	}

	socklen_t len = sizeof(server_address);
	if (getsockname(
		socket_fd,
		reinterpret_cast<sockaddr*>(&server_address),
		&len
	) == -1)
	{
		syserr("getsockname failed");
	}

	// Value used for measuring time since last cleanup.
	// Limiting cleanups to at most 1 per 0.100s (socket timeout) prevents server 
	// from doing too many redundant cleanups.
	auto last_cleanup_time = std::chrono::steady_clock::now();

	uint8_t buffer[BUFFER_SIZE];

	while (true)
	{
		sockaddr_in client_address;
		socklen_t client_address_len = sizeof(client_address);

		ssize_t received_length = recvfrom(
			socket_fd,
			buffer,
			sizeof(buffer),
			0,
			reinterpret_cast<sockaddr*>(&client_address),
			&client_address_len
		);

		auto now = std::chrono::steady_clock::now();

		if (received_length < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// Socket timeout occurred, check for inactive games,
				// clean them up and continue to wait for messages.
				cleanup_inactive_games(config.server_timeout, active_games);
				last_cleanup_time = now;
				continue;
			}
			else
			{
				syswarn("recvfrom failed");
				continue;
			}
		}

		if (now - last_cleanup_time >= std::chrono::milliseconds(TIME_BETWEEN_CLEANUPS_MILISECONDS))        
		{
            cleanup_inactive_games(config.server_timeout, active_games);
            last_cleanup_time = now;
        }

		if (received_length == 0)
		{
			warn("Received empty packet.");
			// Send wrong message to the client with error_index set to 0.
			send_wrong_msg(socket_fd, client_address, buffer, 0, 0);
			continue;
		}

		// Check the first byte of the buffer (msg_type)
		uint8_t msg_type = buffer[0];

		switch (msg_type)
		{
		case MSG_JOIN:
		{
			if (received_length != sizeof(msg_join))
			{
				warn(
					"Invalid length for MSG_JOIN.\nGot "
					+ std::to_string(received_length)
					+ ", expected: "
					+ std::to_string(sizeof(msg_join))
				);
				// Send wrong message to the client with error_index set
				// to either the index of the first missing byte (if the
				// received message is too short) or the index of the
				// first extra byte (if the received message is too long).
				send_wrong_msg(
					socket_fd,
					client_address,
					buffer,
					received_length,
					std::min(
						static_cast<size_t>(sizeof(msg_join)),
						static_cast<size_t>(received_length)
					)
				);
				continue;
			}
			msg_join* message = reinterpret_cast<msg_join*>(buffer);
			handle_join(
				socket_fd,
				message,
				client_address,
				config.max_pawn,
				config.pawn_row,
				active_games,
				next_game_id,
				waiting_game_id
			);
			break;
		}
		case MSG_MOVE_1:
		case MSG_MOVE_2:
		{
			if (received_length != sizeof(msg_move))
			{
				warn(
					"Invalid length for MSG_MOVE.\nGot "
					+ std::to_string(received_length)
					+ ", expected: "
					+ std::to_string(sizeof(msg_move))
				);
				send_wrong_msg(
					socket_fd,
					client_address,
					buffer,
					received_length,
					std::min(
						static_cast<size_t>(sizeof(msg_move)),
						static_cast<size_t>(received_length)
					)
				);
				continue;
			}
			msg_move* message = reinterpret_cast<msg_move*>(buffer);
			handle_move(socket_fd, message, client_address, active_games);
			break;
		}
		case MSG_KEEP_ALIVE:
		{
			if (received_length != sizeof(msg_game_action))
			{
				warn(
					"Invalid length for MSG_KEEP_ALIVE.\nGot "
					+ std::to_string(received_length)
					+ ", expected: "
					+ std::to_string(sizeof(msg_game_action))
				);
				send_wrong_msg(
					socket_fd,
					client_address,
					buffer,
					received_length,
					std::min(
						static_cast<size_t>(sizeof(msg_game_action)),
						static_cast<size_t>(received_length)
					)
				);
				continue;
			}
			handle_keep_alive(
				socket_fd,
				reinterpret_cast<msg_game_action*>(buffer),
				client_address,
				active_games
			);
			break;
		}
		case MSG_GIVE_UP:
		{
			if (received_length != sizeof(msg_game_action))
			{
				warn(
					"Invalid length for MSG_GIVE_UP.\nGot "
					+ std::to_string(received_length)
					+ ", expected: "
					+ std::to_string(sizeof(msg_game_action))
				);
				send_wrong_msg(
					socket_fd,
					client_address,
					buffer,
					received_length,
					std::min(
						static_cast<size_t>(sizeof(msg_game_action)),
						static_cast<size_t>(received_length)
					)
				);
				continue;
			}
			handle_giving_up(
				socket_fd,
				reinterpret_cast<msg_game_action*>(buffer),
				client_address,
				active_games
			);
			break;
		}
		default:
			warn("Unknown msg_type: " + std::to_string(
				static_cast<int>(msg_type)
			));
			// Send wrong message to the client with error_index set to 0.
			send_wrong_msg(
				socket_fd,
				client_address,
				buffer,
				received_length,
				offsetof(client_msg, msg_type)
			);
			break;
		}
	}

	close(socket_fd);
	return 0;
}