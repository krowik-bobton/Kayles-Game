#include "validators.h"
#include "messages.h"
#include <string>
#include <sstream>
#include <cstdint>
#include "err.h"
#include <netdb.h>
#include <cstring>
#include <vector>

uint16_t validate_port(const std::string& str)
{
	if(str.empty())
	{
		fatal("Port number cannot be empty.");
	}
	try
	{
		// std::stoi can throw an exception if the string cannot be
		// converted to an integer (e.g., if it contains non-digit
		// characters)
		size_t ptr;
		int port = std::stoi(str, &ptr);
		if (ptr != str.size())
		{
			fatal("Port number contains invalid characters: '"
				+ str + "'");
		}
		if (port < 1 || port > UINT16_MAX)
		{
			fatal(
				"Port number must be in the range 1 - "
				+ std::to_string(UINT16_MAX)
				+ ", got:" + str
			);
		}
		return static_cast<uint16_t>(port);
	}
	catch (...)
	{
		fatal("Invalid format of the server port: '" + str + "'");
	}
	return 0; // this line will never be reached
}

uint8_t validate_timeout(const std::string& str)
{
	if(str.empty())
	{
		fatal("Timeout value cannot be empty.");
	}
	try
	{
		// std::stoi can throw an exception if the string cannot be
		// converted to an integer
		size_t ptr;
		int timeout = std::stoi(str, &ptr);
		if (ptr != str.size())
		{
			fatal(
				"Timeout value contains invalid characters: '"
				+ str + "'"
			);
		}
		if (timeout < 1 || timeout > 99)
		{
			fatal("Timeout value must be in the range 1 - 99, got: "
				+ std::to_string(timeout));
		}
		return static_cast<uint8_t>(timeout);
	}
	catch (...)
	{
		fatal("Invalid format of the timeout value: '" + str + "'");
	}
	return 0; // this line will never be reached
}

client_msg validate_client_message(const std::string& str)
{
	// words = [msg_type, player_id, game_id, pawn];
	std::string words[4];
	std::stringstream ss(str);

	int number_of_words = 0;
	std::string word;
	while (getline(ss, word, '/'))
	{
		if (number_of_words == 4)
		{
			fatal("Too many fields in message: " + str
				+ ", should be at most 4.");
		}
		words[number_of_words] = word;
		number_of_words++;
	}

	if (number_of_words < 2)
	{
		fatal(
			"Message must contain at least msg_type and player_id "
			"separated with '/'.\n (e.g., 0/123). Got: " + str
		);
	}

	client_msg_type msg_type{};
	uint32_t player_id = 0;
	uint32_t game_id = 0; // Default value if not provided.
	uint8_t pawn = 0;     // Default value if not provided.

	// Convert msg_type
	if (words[0].empty()) fatal("msg_type cannot be empty.");
	try
	{
		size_t ptr;
		int msg_type_int = std::stoi(words[0], &ptr);
		if (ptr != words[0].size())
		{
			fatal("msg_type contains invalid characters: '"
				+ words[0] + "'");
		}
		// check if msg_type is in the valid range of client_msg_type
		if (msg_type_int < MSG_JOIN || msg_type_int > MSG_GIVE_UP)
		{
			fatal(
				"msg_type must be in the range "
				+ std::to_string(MSG_JOIN)
				+ " - "
				+ std::to_string(MSG_GIVE_UP)
				+ ", got: " + words[0]
			);
		}
		msg_type = static_cast<client_msg_type>(msg_type_int);
	}
	catch (...)
	{
		fatal("Invalid format of msg_type: '" + words[0] + "'");
	}

	// Flags indicating whether game_id and pawn fields should be
	// Converted (they are not required for all message types)
	bool convert_game_id = false;
	bool convert_pawn = false;

	// Check if the number of fields is correct for the given msg_type
	std::string msg_type_name = client_msg_type_name(msg_type);
	if (msg_type == MSG_JOIN)
	{
		if (number_of_words != 2)
		{
			fatal(
				"Message " + msg_type_name
				+ " (0) must contain exactly 2 fields "
				"(msg_type/player_id). Got: " + str
			);
		}
	}
	if (msg_type == MSG_KEEP_ALIVE || msg_type == MSG_GIVE_UP)
	{
		if (number_of_words != 3)
		{
			fatal(
				"Message " + msg_type_name
				+ " (3 or 4) must contain exactly 3 fields "
				"(msg_type/player_id/game_id). Got: " + str
			);
		}
		convert_game_id = true;
	}
	if (msg_type == MSG_MOVE_1 || msg_type == MSG_MOVE_2)
	{
		if (number_of_words != 4)
		{
			fatal(
				"Message " + msg_type_name
				+ " (1 or 2) must contain exactly 4 fields "
				"(msg_type/player_id/game_id/pawn). Got: " + str
			);
		}
		convert_game_id = true;
		convert_pawn = true;
	}

	// Convert player_id
	if (words[1].empty()) fatal("player_id cannot be empty.");
	try
	{
		size_t ptr;
		unsigned long long player_id_long = std::stoull(words[1], &ptr);
		if (ptr != words[1].size())
		{
			fatal(
				"player_id contains invalid characters: '"
				+ words[1] + "'"
			);
		}
		if (player_id_long < 1 || player_id_long > UINT32_MAX)
		{
			fatal(
				"player_id must be in the range 1 - "
				+ std::to_string(UINT32_MAX)
				+ ", got: " + words[1]
			);
		}
		player_id = static_cast<uint32_t>(player_id_long);
	}
	catch (...)
	{
		fatal("Invalid format of player_id: '" + words[1] + "'");
	}

	// Convert game_id
	if (convert_game_id)
	{
		if (words[2].empty()) fatal("game_id cannot be empty.");
		try
		{
			size_t ptr;
			unsigned long long game_id_long =
				std::stoull(words[2], &ptr);
			if (ptr != words[2].size())
			{
				fatal(
					"game_id contains invalid characters: '"
					+ words[2] + "'"
				);
			}
			if (game_id_long > UINT32_MAX)
			{
				fatal(
					"game_id must be in the range 0 - "
					+ std::to_string(UINT32_MAX)
					+ ", got: " + words[2]
				);
			}
			game_id = static_cast<uint32_t>(game_id_long);
		}
		catch (...)
		{
			fatal("Invalid format of game_id: '" + words[2] + "'");
		}
	}

	// Convert pawn
	if (convert_pawn)
	{
		if (words[3].empty()) fatal("pawn cannot be empty.");
		try
		{
			size_t ptr;
			int pawn_int = std::stoi(words[3], &ptr);
			if (ptr != words[3].size())
			{
				fatal(
					"pawn contains invalid characters: '"
					+ words[3] + "'"
				);
			}
			if (pawn_int < 0 || pawn_int > UINT8_MAX)
			{
				fatal(
					"pawn value must be in the range 0 - "
					+ std::to_string(UINT8_MAX)
					+ ", got: " + words[3]
				);
			}
			pawn = static_cast<uint8_t>(pawn_int);
		}
		catch (...)
		{
			fatal("Invalid format of pawn: '" + words[3] + "'");
		}
	}

	client_msg result;
	result.msg_type = msg_type;
	result.game_id = game_id;
	result.player_id = player_id;
	result.pawn = pawn;

	return result;
}

std::string validate_address(const std::string& str)
{
	addrinfo hints;
	std::memset(&hints, 0, sizeof(addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	addrinfo* address_result;
	int errcode = getaddrinfo(
		str.c_str(), NULL, &hints, &address_result
	);
	if (errcode != 0)
	{
		fatal(
			"Invalid address, provided: '" + str
			+ "', got error: "
			+ std::string(gai_strerror(errcode))
		);
	}

	// Free the dynamically allocated, linked list address_result
	freeaddrinfo(address_result);
	return str;
}

 // Validates the pawn row string and converts it into a vector of
 // bytes (uint8_t). Each character in the input string should be '0'
 // or '1', where '1' indicates the presence of a pawn and '0' its
 // absence. The first and last characters must be '1' (indicating that
 // the first and last pawns are present).
std::vector<uint8_t> validate_pawn_row(const std::string& str)
{
	if (str.empty())
		fatal("Pawn row cannot be empty.");
	if (str.size() > 256)
	{
		fatal("Pawn row length cannot exceed 256. Got: "
			+ std::to_string(str.size()));
	}
	if (str.front() == '0')
		fatal("The first character in the pawn row must be '1'. Got: " + str);
	if (str.back() == '0')
		fatal("The last character in the pawn row must be '1'. Got: " + str);

	// Compute how many bytes we need to represent the pawn row.
	// Each byte can represent 8 pawns.
	size_t max_pawn = str.size() - 1;
	size_t num_bytes = (max_pawn / 8) + 1;

	// zero the bits
	std::vector<uint8_t> pawn_row(num_bytes, 0);

	for (size_t i = 0; i < str.size(); i++)
	{
		char c = str[i];
		if (c == '1')
		{
			// count the index of the byte and the index of the bit
			// within that byte for the current pawn
			size_t byte_idx = i / 8;
			size_t bit_idx = 7 - (i % 8);
			// set that specific bit.
			pawn_row[byte_idx] |= static_cast<uint8_t>(1 << bit_idx);
		}
		else if (c != '0')
		{
			fatal(
				"Invalid character in pawn row: '"
				+ std::string(1, c)
				+ "'. Pawn row should only contain characters "
				"'0' and '1'. Got: " + str
			);
		}
	}

	return pawn_row;
}