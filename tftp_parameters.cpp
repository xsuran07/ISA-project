/*
 * @author Jakub Šuráň (xsuran07)
 * @file tftp_parameters.h
 * @brief Implementation of tftp_parameters class.
 */

#include <iostream>
#include <regex>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "tftp_parameters.h"

// METHODS FOR DEBUGGING

void Tftp_parameters::print_params()
{
 std::cout << "Request_type: " << this->params.req_type << std::endl;   
 std::cout << "Filename: " << this->params.filename << std::endl;   
 std::cout << "Timeout: " << this->params.timeout << std::endl;   
 std::cout << "Size: " << this->params.size << std::endl;   
 std::cout << "Multicast: " << this->params.multicast << std::endl;   
 std::cout << "Mode: " << this->params.mode << std::endl;
 std::cout << "Address family: " << this->params.addr_family << std::endl;
 std::cout << "Address: " << this->params.address << std::endl;   
 std::cout << "Port: " << this->params.port << std::endl;   
}

// STATIC METHODS

int Tftp_parameters::convert_to_number(std::string str, std::string option = "")
{
    char *end;
    int res = std::strtol(str.c_str(), &end, 10);

    if(*end) {
        std::cerr << option << " may consist of digits only! " << std::endl;
        return -1;       
    }

    if(res <= 0) {
        std::cerr << option << " must be a number larger than 0 (or overflow)!" << std::endl;
        return -1;       
    }

    return res;
}

bool Tftp_parameters::split_string(std::string str, std::string pattern, std::vector<std::string> &vec)
{
    bool ret = true;

    std::regex reg(pattern);
    std::sregex_token_iterator start(str.begin(), str.end(), reg, -1);
    std::sregex_token_iterator end;

    vec.clear();
    vec.insert(vec.end(), start, end);

    //first string is empty
    if(!vec.empty() && vec.begin()->empty()) {
        vec.erase(vec.begin());
        ret = false;
    }

    return ret;
}

// PUBLIC INSTANCE METHODS

// constructor
Tftp_parameters::Tftp_parameters()
{
    init_values();
}

// sets initial values
void Tftp_parameters::init_values()
{
    this->param_with_arg = NOT_DEFINED;
    this->separator = ',';

    this->params.req_type = UNKNOWN;
    this->params.filename = "";
    this->params.timeout = -1;
    this->params.size = 512;
    this->params.multicast = false;
    this->params.mode = BINARY;
    this->params.addr_family = AF_INET;
    this->params.address = "127.0.0.1";
    this->params.port = 69;
}

bool Tftp_parameters::parse(size_t &curr, std::vector<std::string> options)
{
    bool ret;

    // READ from server
    if(options[curr] == "-R") {
        ret = check_req_type(READ);
        this->params.req_type = READ;
    // WRITE to server
    } else if(options[curr] == "-W") {
        ret = check_req_type(WRITE);
        this->params.req_type = WRITE;
    // request multicast
    } else if(options[curr] == "-m") {
        ret = true;
        this->params.multicast = true;
    // file to upload/download
    } else if(options[curr] == "-d") {
        this->param_with_arg = DATA;
        ret = require_arg(curr, options);
    // timeout
    } else if(options[curr] == "-t") {
        this->param_with_arg = TIMEOUT;
        ret = require_arg(curr, options);
    // block size
    } else if(options[curr] == "-s") {
        this->param_with_arg = SIZE;
        ret = require_arg(curr, options);
    // format mode
    } else if(options[curr] == "-c") {
        this->param_with_arg = MODE;
        ret = require_arg(curr, options);
    // address + port
    } else if(options[curr] == "-a") {
        this->param_with_arg = ADDRESS_PORT;
        ret = require_arg(curr, options);
    // invalid option
    } else {
        ret = false;
        std::cerr << "Invalid option \"" << options[curr] << "\"! See help." << std::endl;
    }

    return ret;
}

bool Tftp_parameters::set_properly()
{
    // -R or -W has to be used
    if(this->params.req_type == UNKNOWN) {
        std::cerr << "-R or -W  has to be used!" << std::endl;
        return false;
    }

    // filename has to be specified
    if(this->params.filename.empty()) {
        std::cerr << "File to upload/download has to be specified!" << std::endl;
        return false;
    }

    return true;
}

// PRIVATE INSTANCE METHODS

bool Tftp_parameters::set_address(std::string str)
{
    struct in_addr ipv4_addr;
    struct in6_addr ipv6_addr;

    // valid ipv4 address
    if(inet_pton(AF_INET, str.c_str(), &ipv4_addr) == 1) {
        this->params.addr_family = AF_INET;
    //valid ipv6 address
    } else if(inet_pton(AF_INET6, str.c_str(), &ipv6_addr) == 1) {
        this->params.addr_family = AF_INET6;
    } else {
        std::cerr << "Invalid type address given (neighter ipv4 nor ipv6)!" << std::endl;
        return false;
    }

    this->params.address = str;
    return true;
}

bool Tftp_parameters::set_filename(std::string str)
{
    if(str.empty() || str.at(0) != '/' || str.at(str.size() - 1) == '/') {
        std::cerr << "Invalid form of argument for option -d (HINT absolute path/filename)!" << std::endl;
        return false;
    }

    this->params.filename = str;
    return true;
} 

bool Tftp_parameters::set_mode(std::string str)
{
    if(str == "ascii" || str == "netascii") {
        this->params.mode = ASCII;
    } else if(str == "binary" || str == "octet") {
        this->params.mode = BINARY;
    } else {
        std::cerr << "Unsuported argument for option -c (mode)!" << std::endl;
        return false;
    }

    return true;
}

bool Tftp_parameters::set_port(std::string str)
{
    long ret;

    if((ret = convert_to_number(str, "Port")) < 0 || ret > 65535) {
        return false;
    }

    this->params.port = ret;
    return true;
}

bool Tftp_parameters::set_size(std::string str)
{
    long ret;

    if((ret = convert_to_number(str, "Block size")) < 0) {
        return false;
    }

    if(ret < 8 || ret > 65464) {
        std::cerr << "Only values from range 8-65464 are valid for blksize option!" << std::endl;
        return false;
    }

    this->params.size = ret;
    return true;
}

bool Tftp_parameters::set_timeout(std::string str)
{
    int ret;

    if((ret = convert_to_number(str, "Timeout")) < 0) {
        return false;
    }

    if(ret == 0 || ret > 255) {
        std::cerr << "Only values from range 1-255 are valid for timeout option!" << std::endl;
        return false;
    }

    this->params.timeout = ret;
    return true;
}

bool Tftp_parameters::check_req_type(request_type_t option)
{
    std::vector<std::string> types{ "-R", "-W" };

    if(this->params.req_type != UNKNOWN && this->params.req_type != option) {
        std::cerr <<  "Type of request already specified! Cannot combine \"" 
                << types[option - 1] << "\" with " << types[this->params.req_type - 1] << "." << std::endl;
        return false;
    }

    return true;
}

bool Tftp_parameters::parse_addr_with_sep(size_t curr, std::vector<std::string> options)
{
    std::string str = options[curr];

    if(str[str.size() - 1] == this->separator) {
        str.erase(str.end() - 1);

        return set_address(str) && (curr + 1) < options.size();
    }

    return false;
}

bool Tftp_parameters::parse_port_with_sep(size_t curr, std::vector<std::string> options)
{
    std::string str;

    if(curr + 1 >= options.size()) {
        return false;
    }

    str = options[curr + 1];
    if(str[0] == this->separator) {
        str.erase(str.begin());

        return set_address(options[curr]) && set_port(str);
    }

    return false;
}

bool Tftp_parameters::parse_addr_with_spaces(size_t &curr, std::vector<std::string> options)
{
    std::string sep;

    //need addrses, separator and port number
    if(curr + 2 >= options.size()) {
        return false;
    }

    sep = options[curr + 1];

    //check separator
    if(sep[sep.size() - 1] != this->separator) {
        std::cerr << "Invalid separator!" << std::endl;
        return false;
    }

    return set_address(options[curr]);
}

bool Tftp_parameters::parse_addr_with_port(std::string str)
{
    std::vector<std::string> vec;

    if(!split_string(str, ",", vec) || vec.size() != 2) {
        return false;
    }

    return set_address(vec[0]) && set_port(vec[1]);
}

bool Tftp_parameters::set_address_port(size_t &curr, std::vector<std::string> options)
{
    //<ADDRESS>,<PORT>
    if(parse_addr_with_port(options[curr])) {
        return true;
    //<ADDRESS>, + <PORT>
    } else if(parse_addr_with_sep(curr, options)) {
        curr++;
    } else if(parse_port_with_sep(curr, options)) {
        curr++;
        return true;
    //<ADDRESS> + , + <PORT>
    } else if(parse_addr_with_spaces(curr, options)) {
        curr += 2;
    } else {
        std::cerr << "Invalid arguments for option -a!" << std::endl;
        return false;
    }

    return set_port(options[curr]);
}

bool Tftp_parameters::require_arg(size_t &curr, std::vector<std::string> options)
{
    if(this->param_with_arg == NOT_DEFINED) {
        return false;
    }
    if(curr + 1 >= options.size()) {
        std::cerr << "Option " << options[curr] << " requires argument (see help)!" << std::endl;
        return false;
    }

    curr++;

    switch(this->param_with_arg) {
    case DATA:
        return set_filename(options[curr]);
    case TIMEOUT:
        return set_timeout(options[curr]);
    case SIZE:
        return set_size(options[curr]);
    case MODE:
        return set_mode(options[curr]);
    case ADDRESS_PORT:
        return set_address_port(curr, options);
    default:
        return false;
    }

}