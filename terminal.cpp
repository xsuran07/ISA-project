/*
 * @author Jakub Šuráň (xsuran07)
 * @file terminal.cpp
 * @brief Implementation of class representing terminal.
 */

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
        this->client.communicate(this->p.get_params());
        return true;
    case Parser::INVALID:
        return true;
    default:
        return false;
    }
}

void Terminal::run()
{
    bool run = true;


	while(run) {
		std::cout << ">";
		std::getline(std::cin, this->line);

        run = perform_command();
	}
}