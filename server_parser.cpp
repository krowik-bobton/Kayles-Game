#include <string>
#include <unistd.h>
#include "server_parser.h"
#include "err.h"
#include "validators.h"

parsed_server_data parse_server_data(int argc, char* argv[])
{
	parsed_server_data result_data;
	const std::string USAGE_MSG =
		"\nUsage: ./program -r <pawn_row> -a <address>"
		" -p <port> -t <timeout>";

	// Values indicating whether a flag was provided.
	bool has_pawn_row = false;
	bool has_address  = false;
	bool has_port     = false;
	bool has_timeout  = false;
	int opt;

	while ((opt = getopt(argc, argv, ":r:a:p:t:")) != -1)
	{
		switch (opt)
		{
		case 'r':
			if (has_pawn_row)
				fatal("Duplicate flag: -r <pawn_row>");
			result_data.pawn_row = validate_pawn_row(optarg);
			result_data.max_pawn = static_cast<uint8_t>(
				std::string(optarg).length() - 1
			);
			has_pawn_row = true;
			break;
		case 'a':
			if (has_address)
				fatal("Duplicate flag: -a <address>");
			result_data.address = validate_address(optarg);
			has_address = true;
			break;
		case 'p':
			if (has_port)
				fatal("Duplicate flag: -p <port>");
			if (std::string(optarg) == "0")
			{
				// Manually check this case, because validate_port
				// is only for ports 1 - UINT16_MAX.
				result_data.port = 0;
			}
			else
			{
				result_data.port = validate_port(optarg);
			}
			has_port = true;
			break;
		case 't':
			if (has_timeout)
				fatal("Duplicate flag: -t <timeout>");
			result_data.server_timeout = validate_timeout(optarg);
			has_timeout = true;
			break;
		case ':':
			fatal(
				"Missing argument for flag: -"
				+ std::string(1, static_cast<char>(optopt))
				+ USAGE_MSG
			);
			break;
		case '?':
			fatal(
				"Unknown flag: -"
				+ std::string(1, static_cast<char>(optopt))
				+ USAGE_MSG
			);
			break;
		}
	}

	if (optind < argc)
	{
		fatal(
			"Unexpected non-flag argument: '"
			+ std::string(argv[optind])
			+ "'"
			+ USAGE_MSG
		);
	}

	if (!has_pawn_row)
		fatal("Missing required flag: -r <pawn_row>" + USAGE_MSG);
	if (!has_address)
		fatal("Missing required flag: -a <address>" + USAGE_MSG);
	if (!has_port)
		fatal("Missing required flag: -p <port>" + USAGE_MSG);
	if (!has_timeout)
		fatal("Missing required flag: -t <timeout>" + USAGE_MSG);

	return result_data;
}