#ifndef __TFTP_PARAMETERS_H_
#define __TFTP_PARAMETERS_H_

#include <string>
#include <vector>
#include <stdint.h>

class Tftp_parameters
{
    public:
        typedef enum {
            NOT_DEFINED,
            DATA,
            TIMEOUT,
            SIZE,
            MODE,
            ADDRESS_PORT,
        } req_arg_t;

        typedef enum {
            UNKNOWN,
            READ,
            WRITE,
        } request_type_t;

        typedef enum {
            ASCII,
            BINARY,
        } transfer_mode_t;

        typedef struct {
            request_type_t req_type;
            std::string filename;
            int timeout;
            uint64_t size;
            bool multicast;
            transfer_mode_t mode;
            std::string address;
            uint16_t port;
        } params_t;

    private:
        params_t params;
        req_arg_t param_with_arg;
    public:
        Tftp_parameters();

        void init_values();
        bool parse(size_t &curr, std::vector<std::string> options);
        void print_params();

    private:
        long convert_to_number(std::string str, std::string option);
        bool validate_ipv4_address(std::string str);
        bool validate_ipv6_address(std::string str);

        bool check_req_type(request_type_t option);
        bool require_arg(size_t &curr, std::vector<std::string> options);
        bool get_filename(std::string str);
        bool get_timeout(std::string str);
        bool get_size(std::string str);
        bool get_mode(std::string str);
        bool get_address_port(std::string str);
};

#endif