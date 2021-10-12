/**
 * @author Jakub Šuráň (xsuran07)
 * @file terminal.h
 * @brief Interface of class representing terminal.
 */

#ifndef __TERMINAL_H_
#define __TERMINAL_H_

#include <string>

#include "parser.h"
#include "tftp_client.h"

/**
 * @brief Class representing terminal which suppport
 * only very limitied set of commands - HELP, QUIT and
 * TFTP client.
 */
class Terminal
{
    private:
        std::string line;
        Parser p;
        Tftp_client client;

    public:
        /**
         * @brief Constructor
         */
        Terminal() {};

        /**
         * @brief Starts infinite loop which
         * reads commands and tries to perform them.
         */
        void run();

    private:
        /**
         * @brief Tries to parse and perform read command.
         * @returns true if command has been successfuly parsed
         * and perform, false otherwise
         */
        bool perform_command();

};

#endif