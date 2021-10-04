#include <iostream>
#include <string>
#include <vector>

#include "terminal.h"

bool Terminal::perform_command()
{
    this->p.set_options(this->line);

    switch(this->p.parse_command()) {
    case Parser::HELP:
        std::cout << "Help TODO!" << std::endl;
        return true;
    case Parser::QUIT:
        return false;
    case Parser::TFTP:
        std::cout << "TFTP" << std::endl;
        return true;
    case Parser::INVALID:
        return true;
    default:
        return false;
    }
}

int Terminal::run()
{
    bool run = true;

	while(run) {
		std::cout << ">";
		std::getline(std::cin, this->line);

        run = perform_command();
	}

    return 0;
}