#include "err.h"
#include "validators.h"
#include "client_parser.h"
#include "common.h"

#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <iostream>
#include <unistd.h>
#include <cstddef>

constexpr size_t BUFFER_SIZE = 512;

static void send_to_server(
	int socket_fd,
	const client_msg& message,
	const sockaddr_in& server_address
)
{
	ssize_t sent_length = 0;
	ssize_t to_send_length = 0;

	// Check message types and send only neccessary data
	switch (message.msg_type)
	{
	case MSG_JOIN:
	{
		// MSG_JOIN requires only msg_type and player_id.
		msg_join packet;
		packet.msg_type = message.msg_type;
		packet.player_id = htonl(message.player_id);
		to_send_length = sizeof(packet);
		sent_length = sendto(
			socket_fd,
			&packet,
			sizeof(packet),
			0,
			reinterpret_cast<sockaddr*>(
				const_cast<sockaddr_in*>(&server_address)
			),
			sizeof(server_address)
		);
		break;
	}
	case MSG_KEEP_ALIVE:
	case MSG_GIVE_UP:
	{
		// Action messages require only msg_type, player_id and game_id.
		msg_game_action packet;
		packet.msg_type = message.msg_type;
		packet.player_id = htonl(message.player_id);
		packet.game_id = htonl(message.game_id);
		to_send_length = sizeof(packet);
		sent_length = sendto(
			socket_fd,
			&packet,
			sizeof(packet),
			0,
			reinterpret_cast<sockaddr*>(
				const_cast<sockaddr_in*>(&server_address)
			),
			sizeof(server_address)
		);
		break;
	}
	case MSG_MOVE_1:
	case MSG_MOVE_2:
	{
		// MOVE_* messages require all fields: msg_type, player_id,
		// game_id and pawn.
		msg_move packet;
		packet.msg_type = message.msg_type;
		packet.player_id = htonl(message.player_id);
		packet.game_id = htonl(message.game_id);
		packet.pawn = message.pawn;
		to_send_length = sizeof(packet);
		sent_length = sendto(
			socket_fd,
			&packet,
			sizeof(packet),
			0,
			reinterpret_cast<sockaddr*>(
				const_cast<sockaddr_in*>(&server_address)
			),
			sizeof(server_address)
		);
		break;
	}
	default:
		fatal("Unknown message type");
	}

	if (sent_length < 0)
		syserr("sendto failed while sending message to the server");
	if (sent_length != to_send_length)
		syserr("incomplete sending");
}

int main(int argc, char* argv[])
{
	// Parse and validate the command line arguments.
	// If the arguments are invalid, the above function will exit
	// with an error message.
	parsed_client_data input_data = parse_input_client(argc, argv);

	// Get the server address structure.
	sockaddr_in server_address =
		get_address(input_data.address, input_data.port);

	int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd < 0)
		syserr("cannot create a socket for sending the message to the server.");

	// Set a timeout on the socket
	struct timeval timeout = {
		.tv_sec = input_data.server_timeout,
		.tv_usec = 0
	};

	if (setsockopt(
		socket_fd,
		SOL_SOCKET,
		SO_RCVTIMEO,
		&timeout,
		sizeof(timeout)
	) < 0)
	{
		syserr("cannot set a timeout on the client socket.");
	}

	send_to_server(socket_fd, input_data.message_struct, server_address);

	// Receive response from a server
	uint8_t buffer[BUFFER_SIZE];
	sockaddr_in sender_address;
	socklen_t sender_address_len = sizeof(sender_address);

	ssize_t received_length = recvfrom(
		socket_fd,
		buffer,
		sizeof(buffer),
		0,
		reinterpret_cast<sockaddr*>(&sender_address),
		&sender_address_len
	);

	if (received_length < 0)
	{
		// Response not received, print an information and return
		// with code 0
		std::cout << "No response from server within "
			<< static_cast<int>(input_data.server_timeout)
			<< " seconds." << std::endl;
		close(socket_fd);
		return 0;
	}

	// Server can send back either game_state or wrong_msg structure.
	// Both structures have at least 14 bytes (base size), and their
	// 13th byte (index 12) is a status field.
	if (static_cast<size_t>(received_length) < sizeof(game_state))
	{
		fatal(
			"Received message is too short to be valid. Expected at least "
			+ std::to_string(sizeof(game_state))
			+ " bytes, but received "
			+ std::to_string(received_length)
			+ " bytes."
		);
	}

	uint8_t status = buffer[offsetof(game_state, status)];

	if (status == MSG_WRONG_MSG)
	{
		// Server considered client's message wrong.
		if (received_length != sizeof(wrong_msg))
		{
			fatal(
				"Received message with status MSG_WRONG_MSG, but received "
				"number of bytes: "
				+ std::to_string(received_length)
				+ " doesn't match the expected: "
				+ std::to_string(sizeof(wrong_msg))
				+ " bytes."
			);
		}
		wrong_msg received_msg;
		std::memcpy(&received_msg, buffer, sizeof(received_msg));

		std::cout << "\n--- WRONG MESSAGE ("
			<< server_msg_type_name(
				static_cast<server_msg_type>(status)
			)
			<< ") ---\n";
		std::cout << "Server couldn't interpret the byte at the index: "
			<< static_cast<int>(received_msg.error_index)
			<< " (0-indexed)\n";
		std::cout << "From the message it received:\n[";

		for (int i = 0; i < 12; ++i)
		{
			std::cout << static_cast<int>(received_msg.original_msg[i])
				<< (i < 11 ? " " : "");
		}
		std::cout << "]\n"
			<< "---------------------------------------------\n";
	}
	else if (
		server_msg_type_name(static_cast<server_msg_type>(status))
		== "UNKNOWN_MSG"
	)
	{
		fatal(
			"Received message with unknown status: "
			+ std::to_string(status)
			+ ". The message is invalid."
		);
	}
	else
	{
		// Server sent a game state.
		game_state state;

		// Copy only fields with constant size (pawn_row is a
		// dynamically allocated array of bytes).
		std::memcpy(&state, buffer, sizeof(game_state));

		// Translate the endianess (skip 1-byte fields)
		state.game_id = ntohl(state.game_id);
		state.player_a_id = ntohl(state.player_a_id);
		state.player_b_id = ntohl(state.player_b_id);

		// Verify the size of pawn_row
		size_t expected_pawn_row_size = (state.max_pawn / 8) + 1;
		if (static_cast<size_t>(received_length)
			!= sizeof(game_state) + expected_pawn_row_size)
		{
			fatal(
				"Message length mismatch. Expected: "
				+ std::to_string(
					sizeof(game_state) + expected_pawn_row_size
				)
				+ ", got: "
				+ std::to_string(received_length)
			);
		}

		std::cout << "\n--- GAME STATE ---\n";
		std::cout << "Game ID   : " << state.game_id << "\n";
		std::cout << "Player A  : " << state.player_a_id << "\n";
		std::cout << "Player B  : "
			<< (state.player_b_id == 0
				? "0 [Not connected yet]"
				: std::to_string(state.player_b_id))
			<< "\n";
		std::cout << "Status    : "
			<< server_msg_type_name(
				static_cast<server_msg_type>(state.status)
			)
			<< "\n";

		std::cout << "Pawn Row  : ";
		for (int i = 0; i <= state.max_pawn; i++)
		{
			size_t byte_idx = sizeof(game_state) + (i / 8);
			size_t bit_idx = 7 - (i % 8);
			bool is_pawn =
				(buffer[byte_idx] & (1 << bit_idx)) != 0;
			std::cout << (is_pawn ? "1" : "0");
		}
		std::cout << "\n------------------\n\n";
	}

	close(socket_fd);
	return 0;
}