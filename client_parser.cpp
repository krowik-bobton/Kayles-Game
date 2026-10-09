#include <string>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <iostream>

#include "client_parser.h"
#include "err.h"
#include "validators.h"

parsed_client_data parse_input_client(int argc, char* argv[])
{
	parsed_client_data result_data;
	const std::string USAGE_MSG =
		"\nUsage: ./program -a <address> -p <port>"
		" -m <message> -t <timeout>";

	std::memset(
		&result_data.message_struct,
		0,
		sizeof(client_msg)
	);

	bool has_address = false;
	bool has_port    = false;
	bool has_message = false;
	bool has_timeout = false;
	int opt;

	while ((opt = getopt(argc, argv, ":a:p:m:t:")) != -1)
	{
		switch (opt)
		{
		case 'a':
			if (has_address)
				fatal("Duplicate flag: -a <address>");
			result_data.address = validate_address(optarg);
			has_address = true;
			break;
		case 'p':
			if (has_port)
				fatal("Duplicate flag: -p <port>");
			result_data.port = validate_port(optarg);
			has_port = true;
			break;
		case 't':
			if (has_timeout)
				fatal("Duplicate flag: -t <timeout>");
			result_data.server_timeout = validate_timeout(optarg);
			has_timeout = true;
			break;
		case 'm':
			if (has_message)
				fatal("Duplicate flag: -m <message>");
			result_data.message_struct =
				validate_client_message(optarg);
			has_message = true;
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
				"Unknown option: -"
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

	if (!has_address)
		fatal("Missing required flag: -a <address>" + USAGE_MSG);
	if (!has_port)
		fatal("Missing required flag: -p <port>" + USAGE_MSG);
	if (!has_message)
		fatal("Missing required flag: -m <message>" + USAGE_MSG);
	if (!has_timeout)
		fatal("Missing required flag: -t <timeout>" + USAGE_MSG);

	return result_data;
}