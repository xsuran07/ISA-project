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
    bool error = false;

    this->params.init_values();


    while(!error && i < this->opt_count) {
        if(this->options[i] == "HELP") {
            ret = HELP;
            error = check_combination(ret, this->options[i]);
        } else if(this->options[i] == "QUIT") {
            ret = QUIT;
            error = check_combination(ret, this->options[i]);
        } else {
            error = this->params.parse(i, this->options);
        }

        i++;
    }

    this->params.print_params(); 
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
        return true;
    }

    return false;
}