/*
 * Copyright (c) 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <logger/Logging.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <cstdint>

namespace po = boost::program_options;

namespace ember {

void launch(const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);

} // ember

int main(int argc, const char* argv[]) try {
	using namespace ember;

	const po::variables_map args = parse_arguments(argc, argv);
	launch(args);
} catch(std::exception& e) {
	std::cerr << e.what();
	return 1;
}

namespace ember {

void launch(const po::variables_map& args) {

}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description opt("Options");

	opt.add_options()
		("help,h", "Displays a list of available options")
		("server.host", po::value<std::string>()->required(),
			"Login server hostname or IP"),
		("server.port", po::value<std::uint16_t>()->required(),
			"Login server port number"),
		("server.name", po::value<std::string>()->default_value(""),
			"Gateway name, or leave blank for random selection"),
		("client.threads", po::value<unsigned int>()->default_value(0),
			"Number of threads - 0 to let the tool choose"),
		("client.max_connections", po::value<std::uint16_t>()->default_value(0),
			"Max. number of connections, limited by available ports"),
		("client.retry", po::bool_switch()->default_value(false),
			"Reopen a connection if closed by the server"),
		("metrics.host", po::value<std::string>()->default_value(""),
		   "Hostname for a metrics backend server"),
		("metrics.port", po::value<std::uint16_t>()->default_value(0),
		   "Ports for a metrics backend server");

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).options(opt)
	          .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
	          .run(), options);
	po::notify(options);

	if(options.count("help") || argc <= 1) {
		std::cout << opt << "\n";
		std::exit(0);
	}

	return options;
}

} // ember