#ifndef __TERMINAL_H_
#define __TERMINAL_H_

#include <string>

#include "parser.h"

class Terminal
{
    private:
        std::string line;
        Parser p;

    public:
        Terminal() {};

        int run();

    private:
        bool perform_command();

};

#endif