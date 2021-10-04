/*
 * @author Jakub Šuráň (xsuran07)
 * @file tftp_parameters.h
 * @brief Interface of tftp_parameters class.
 */

#ifndef __TFTP_PARAMETERS_H_
#define __TFTP_PARAMETERS_H_

#include <string>
#include <vector>
#include <stdint.h>

/**
 * @brief Class for parsing and validating parameters for
 * tftp client.
 */
class Tftp_parameters
{
    private:
        /**
         * @brief Enumaration of parameters which
         * requires at least one argument.
         */
        typedef enum {
            NOT_DEFINED,
            DATA,
            TIMEOUT,
            SIZE,
            MODE,
            ADDRESS_PORT,
        } req_arg_t;

    public:
        /**
         * @brief Enumaration of request types for TFTP client.
         */
        typedef enum {
            UNKNOWN,
            READ,
            WRITE,
        } request_type_t;

        /**
         * @brief Types of data format modes.
         */
        typedef enum {
            ASCII,
            BINARY,
        } transfer_mode_t;

        /**
         * @brief Structure with parameters for TFTP client's request.
         */
        typedef struct {
            request_type_t req_type;
            std::string filename;
            int timeout;
            uint64_t size;
            bool multicast;
            transfer_mode_t mode;
            int addr_family;
            std::string address;
            uint16_t port;
        } params_t;

    private:
        params_t params;
        req_arg_t param_with_arg;
        char separator;
    public:
        Tftp_parameters();

        void init_values();
        bool parse(size_t &curr, std::vector<std::string> options);
        bool set_properly();
        void print_params();

        static long convert_to_number(std::string str, std::string option);
        static bool split_string(std::string str, std::string pattern, std::vector<std::string> &vec);

    private:
        bool set_address(std::string str);
        bool set_filename(std::string str);
        bool set_port(std::string str);
        bool set_mode(std::string str);
        bool set_size(std::string str);
        bool set_timeout(std::string str);
        bool set_address_port(size_t &curr, std::vector<std::string> options);

        bool check_req_type(request_type_t option);
        bool require_arg(size_t &curr, std::vector<std::string> options);
        
        bool parse_addr_with_port(std::string str);
        bool parse_addr_with_sep(size_t curr, std::vector<std::string> options);
        bool parse_port_with_sep(size_t curr, std::vector<std::string> options);
        bool parse_addr_with_spaces(size_t &curr, std::vector<std::string> options);
};

#endif