#include <iostream>
#include <regex>

#include "parser.h"

Parser::Parser()
{
    this->opt_count = 0;
}

void Parser::set_options(std::string &str)
{
    std::regex reg("\\s+"); //match whitespace sequences
    std::sregex_token_iterator start(str.begin(), str.end(), reg, -1);
    std::sregex_token_iterator end;

    this->options.clear();
    this->options.insert(this->options.end(), start, end);
    this->opt_count = this->options.size();

    std::cout << this->opt_count << std::endl;
}
        
Parser::command_t Parser::parse_command()
{
    if(this->opt_count == 0) {
        return INVALID;
    }

    Parser::command_t ret = INVALID;
    size_t i = (this->options[0].empty())? 1 : 0;
    bool no_error = true;

    this->params.init_values();


    while(no_error && i < this->opt_count) {
        if(this->options[i] == "HELP") {
            ret = HELP;
            no_error = check_combination(ret, this->options[i]);
        } else if(this->options[i] == "QUIT") {
            ret = QUIT;
            no_error = check_combination(ret, this->options[i]);
        } else if(this->params.parse(i, this->options)) {
            ret = TFTP;
            std::cout << "fsdfsd\n";
        } else {
            ret = INVALID;
            no_error = false;
        }

        i++;
    }

    this->params.print_params(); 

    // check validity of TFTP parameters
    if(ret == TFTP && !this->params.set_properly()) {
        ret = INVALID;
    }

    return ret;
}

bool Parser::check_combination(Parser::command_t &opt, std::string option)
{
    //don't take into account first empty option
    size_t expected = (this->options[0].empty())? 2 : 1;

    //option cannot be combined
    if(this->opt_count != expected) {
        opt = INVALID;
        std::cerr << "Option " << option << " cannot be combined with other options!" << std::endl;
        return false;
    }

    return true;
}