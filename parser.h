/*
 * @author Jakub Šuráň (xsuran07)
 * @file parser.h
 * @brief Interface of class representing command parser.
 */

#ifndef __PARSER_H_
#define __PARSER_H_

#include <string>
#include <vector>

#include "tftp_parameters.h"

/**
 * @brief Class representing simple command parser.
 */
class Parser
{
    public:
        typedef enum {
            HELP,
            QUIT,
            TFTP,
            INVALID,
        } command_t;

    private:
        size_t opt_count;
        std::vector<std::string> options;
        Tftp_parameters params;

    public:
        /**
         * @brief Constructor.
         */
        Parser();

        /**
         * @brief Splits given string by whitespaces into
         * internal vector of strings.
         * @param str String with commands.
         */
        void set_options(std::string &str);

        /**
         * @brief Tries to parse commands stored in internal
         * vector of strings.
         * @returns type of parse command or INVALID if no valid
         * command could be parsed.
         */
        command_t parse_command();

    private:
        /**
         * @brief Checks whether internal vector of strings includes only
         * one command. This is a convenient function for command
         * which cannot be combined with others.
         * @returns true if internal vector of string includes only one
         * command, false otherwise.
         */
        bool check_combination(command_t &opt, std::string option);
};

#endif