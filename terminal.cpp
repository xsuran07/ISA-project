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

    // performs requested command
    switch(this->p.parse_command()) {
    case Parser::HELP:
        print_help();
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

    // reads commands and parses them till not ended
	while(run) {
		std::cout << ">";
		std::getline(std::cin, this->line);

        if(std::cin.eof()) {
            break;
        }

        run = perform_command();
	}
}

void Terminal::print_help()
{
    std::cout << "Welcome to interactive console of mytftpclient - simple TFTP client" << std::endl;
    std::cout << "Supported commands:" << std::endl;
    std::cout << "* help - print this help" << std::endl;
    std::cout << "* quit - ends interactive terminal mode, terminal also ends when EOF is read" << std::endl;
    std::cout << "* [TFTP request parameters] - specification of parameters for TFTP request:" << std::endl;
    std::cout << "\t -R - request reading from server (required if -W isn't used, usage of both is forbidden)" << std::endl;
    std::cout << "\t -W - request writing to server (required if -R isn't used, usage of both is forbidden)" << std::endl;
    std::cout << "\t -d /absolute_path/filename - filename specifis name of file to transfer,"
        << " absolute_path specifies location of file on server (reqired)" << std::endl;
    std::cout << "\t -t timeout - specifies timeout in second, which will be proposed to server - RFC 2348 (optional)" << std::endl;
    std::cout << "\t -s blksize - blocksize, which will be proposed to server - RFC 2347; if not used"
        << " value of 512 bytes is default (optional)" << std::endl;
    std::cout << "\t -m request multicast transfer - RFC 2090 (optional)" << std::endl;
    std::cout << "\t -c mode - specifies tranfer mode - allowed values for 'mode' are ascii"
        << " (or netascii) and binary (or octet) (optional)" << std::endl;
    std::cout << "\t -a address, port - address specifies server address (may be both ipv4 or ipv6); default is 127.0.0.1,"
        << " port specifies port number server listens on; default value is 69 (optional)" << std::endl;
}