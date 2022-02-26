module;

#include "Version.h"
#include "util/ConsoleColour.h"
#include <iostream>

export module Shared.Banner;

export import <string_view>;

namespace ember {

export void print_banner(std::string_view display_name) {
	util::ConsoleColour console;

	console.set(util::Colour::DARK_GREY);
	std::cout << "\n"
		R"(                                      d8b)" << "\n";
	console.set(util::Colour::GREY);
	std::cout <<
		R"(                                      ?88)" << "\n";
	console.set(util::Colour::YELLOW);
	std::cout <<
		R"(                                       88b)" << "\n"
		R"(       )         d8888b  88bd8b,d88b   888888b  d8888b  88bd88b)" << "\n";
	console.set(util::Colour::LIGHT_RED);
	std::cout <<
		R"(      ) \       d8b_,dP  88P'`?8P'?8b  88P `?8bd8b_,dP  88P'  `)" << "\n";
	console.set(util::Colour::RED);
	std::cout <<
		R"(     / ) (      88b     d88  d88  88P d88,  d8888b     d88)" << "\n"
		R"(     \(_)/      `?888P'd88' d88'  88bd88'`?88P'`?888P'd88')" << "\n\n";

	console.reset();

	std::cout << display_name << ", v" << version::VERSION << " (" << version::GIT_HASH << ")\n\n";
}

} // ember