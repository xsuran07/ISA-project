#include <iostream>
#include <regex>

#include "tftp_parameters.h"

Tftp_parameters::Tftp_parameters()
{
    init_values();
}

void Tftp_parameters::init_values()
{
    this->param_with_arg = NOT_DEFINED;

    this->params.req_type = UNKNOWN;
    this->params.filename = "";
    this->params.timeout = -1;
    this->params.size = 512;
    this->params.multicast = false;
    this->params.mode = BINARY;
    this->params.address = "127.0.0.1";
    this->params.port = 69;
}

void Tftp_parameters::print_params()
{
 std::cout << "Request_type: " << this->params.req_type << std::endl;   
 std::cout << "Filename: " << this->params.filename << std::endl;   
 std::cout << "Timeout: " << this->params.timeout << std::endl;   
 std::cout << "Size: " << this->params.size << std::endl;   
 std::cout << "Multicast: " << this->params.multicast << std::endl;   
 std::cout << "Mode: " << this->params.mode << std::endl;   
 std::cout << "Address: " << this->params.address << std::endl;   
 std::cout << "Port: " << this->params.port << std::endl;   
}

bool Tftp_parameters::check_req_type(request_type_t option)
{
    bool ret = false;
    std::vector<std::string> types{ "-R", "-W" };

    if(this->params.req_type != UNKNOWN && this->params.req_type != option) {
        ret = false;
        std::cerr <<  "Type of request already specified! Cannot combine \"" 
                << types[option - 1] << "\" with " << types[this->params.req_type - 1] << "." << std::endl;
    }

    return ret;
}

bool Tftp_parameters::get_filename(std::string str)
{
    bool ret = false;
    this->params.filename = str;

    if(str.empty() || str.at(0) != '/') {
        ret = true;
        std::cerr << "Invalid form of argument for option -d (HINT absolute path)!" << std::endl;
    }

    return ret;
} 

long Tftp_parameters::convert_to_number(std::string str, std::string option)
{
    char *end;
    long res = std::strtol(str.c_str(), &end, 10);

    if(*end) {
        std::cerr << "Argument for option " << option << " may consist of digits only! " << std::endl;
        return -1;       
    }

    if(res <= 0) {
        std::cerr << "Argument for option " << option << " must be a number larger than 0" << std::endl;
        return -1;       
    }

    return res;
}

bool Tftp_parameters::get_timeout(std::string str)
{
    this->params.timeout = convert_to_number(str, "-t");
    
    return this->params.timeout < 0;
}

bool Tftp_parameters::get_size(std::string str)
{
    long ret;

    if((ret =convert_to_number(str, "-s")) < 0) {
        return true;
    }

    this->params.size = ret;
    return false;
}

bool Tftp_parameters::get_mode(std::string str)
{
    bool ret = false;

    if(str == "ascii" || str == "netascii") {
        this->params.mode = ASCII;
    } else if(str == "binary" || str == "octet") {
        this->params.mode = BINARY;
    } else {
        std::cerr << "Unsuported argument for option -c (mode)!" << std::endl;
        ret = true;
    }

    return ret;
}

bool Tftp_parameters::validate_ipv4_address(std::string str)
{
    std::regex ipv4_addr(
        "^(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])$"
    );

    return std::regex_match(str, ipv4_addr);
}

bool Tftp_parameters::validate_ipv6_address(std::string str)
{
    std::regex ipv6_addr(
        "^((([0-9a-fA-F]){1,4})\\:){7}([0-9a-fA-F]){1,4}$"
    );

    return std::regex_match(str, ipv6_addr);
}

bool Tftp_parameters::get_address_port(std::string str)
{
    if(validate_ipv4_address(str)) {
        this->params.address = str;
        return false;
    } else if(validate_ipv6_address(str)) {
        this->params.address = str;
        return false;
    }

    std::cerr << "Invalid address (IPV4 nor IPV6)!" << std::endl;
    return true;

}

bool Tftp_parameters::require_arg(size_t &curr, std::vector<std::string> options)
{
    if(this->param_with_arg == NOT_DEFINED) {
        return false;
    }

    if(curr + 1 >= options.size()) {
        std::cerr << "Option " << options[curr] << " requires argument (see HELP)!" << std::endl;
        return false;
    }

    curr++;

    switch(this->param_with_arg) {
    case DATA:
        return get_filename(options[curr]);
    case TIMEOUT:
        return get_timeout(options[curr]);
    case SIZE:
        return get_size(options[curr]);
    case MODE:
        return get_mode(options[curr]);
    case ADDRESS_PORT:
        return get_address_port(options[curr]);
    default:
        return false;
    }

}

bool Tftp_parameters::parse(size_t &curr, std::vector<std::string> options)
{
    bool ret;

    if(options[curr] == "-R") { // READ from server
        ret = check_req_type(READ);
        this->params.req_type = READ;
    } else if(options[curr] == "-W") { // WRITE to server
        ret = check_req_type(WRITE);
        this->params.req_type = WRITE;
    } else if(options[curr] == "-m") { // request multicast
        ret = false;
        this->params.multicast = true;
    } else if(options[curr] == "-d") {
        this->param_with_arg = DATA;
        ret = require_arg(curr, options);
    } else if(options[curr] == "-t") {
        this->param_with_arg = TIMEOUT;
        ret = require_arg(curr, options);
    } else if(options[curr] == "-s") {
        this->param_with_arg = SIZE;
        ret = require_arg(curr, options);
    } else if(options[curr] == "-c") {
        this->param_with_arg = MODE;
        ret = require_arg(curr, options);
    } else if(options[curr] == "-a") {
        this->param_with_arg = ADDRESS_PORT;
        ret = require_arg(curr, options);
    } else {
        ret = true; //invalid option
        std::cerr << "Invalid option \"" << options[curr] << "\"" << std::endl;
    }

    return ret;
}