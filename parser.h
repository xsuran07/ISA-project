#ifndef __PARSER_H_
#define __PARSER_H_

#include <string>
#include <vector>

#include "tftp_parameters.h"

class Parser
{
    private:
        size_t opt_count;
        std::vector<std::string> options;
        Tftp_parameters params;

    public:
        typedef enum {
            HELP,
            QUIT,
            INVALID,
        } command_t;

        Parser();

        void set_options(std::string &str);
        command_t parse_command();

    private:
        bool check_combination(command_t &opt, std::string option);
};

#endif