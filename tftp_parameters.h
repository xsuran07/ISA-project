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
            request_type_t req_type; // determines type of request to server (READ or WRITE)
            std::string filename; // abs_path/file to send/recieved (abs_path on server)
            int timeout; // timeout for tftp communication
            uint64_t size; // size of data block for tftp communication
            bool multicast;
            transfer_mode_t mode; // determines data encoding (BINARY or NETASCII)
            int addr_family;
            std::string address;
            uint16_t port;
        } params_t;

    private:
        params_t params;
        req_arg_t param_with_arg;
        char separator;

    public:
        /**
         * @brief Constructor.
         */
        Tftp_parameters();


        /**
         * @brief Getter for address attribute.
         */
        std::string get_address() { return this->params.address; };

        /**
         * @brief Getter for addr_family attribute.
         */
        int get_addr_family() { return this->params.addr_family; };

        /**
         * @brief Getter for filename attribute.
         */
        std::string get_filename() { return this->params.filename; };

        /**
         * @brief Getter for port attribute.
         */
        uint16_t get_port() { return this->params.port; };

        /**
         * @brief Getter for mode attribute.
         */
        transfer_mode_t get_mode() { return this->params.mode; };

        /**
         * @brief Getter for req_type attribute.
         */
        request_type_t get_req_type() { return this->params.req_type; };

        /**
         * @brief Getter for size attribute.
         */
        uint64_t get_size() {return this->params.size; };

        /**
         * @brief Getter for timeout attribute.
         */
        int get_timeout() { return this->params.timeout; };

        /**
         * @brief Sets default values to all parameters.
         */
        void init_values();

        /**
         * @brief Try to parse options in given options and extract
         * information from them.
         * @param curr Current position in vector with options.
         * @param options Vector with options to parse.
         * @returns true if parsing succeeds, false otherwise.
         */
        bool parse(size_t &curr, std::vector<std::string> options);

        /**
         * @brief Checks if parameters are set properly - all required options
         * has been supplied, etc.
         * @returns true if parameters are set properly, false otherwise.
         */
        bool set_properly();

        /**
         * @brief Debugging function.
         */
        void print_params();

        /**
         * @brief Converts given string to number. String should contain
         * unsigned number, otherwise conversion will fail.
         * @param str String to convert to number.
         * @param option String to add into error message (optional).
         * @returns Converted number if successful, -1 on error.
         */
        static long convert_to_number(std::string str, std::string option);

        /**
         * @brief Splits given string with specified pattern and stores parts into given
         * vector.
         * @param str String to split.
         * @param pattern Pattern to split string with.
         * @param vec Vectore where result will be stored.
         * @returns true if splitting succeeds, false otherwise.
         */
        static bool split_string(std::string str, std::string pattern, std::vector<std::string> &vec);

    private:
        /**
         * @brief Validates correctness of given address and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_address(std::string str);

        /**
         * @brief Validates correctness of given filename and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_filename(std::string str);

        /**
         * @brief Validates correctness of given port and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_port(std::string str);

        /**
         * @brief Validates correctness of given given transfer mode and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_mode(std::string str);

        /**
         * @brief Validates correctness of given block size and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_size(std::string str);

        /**
         * @brief Validates correctness of given timeout and stores it into
         * appropriate attribute.
         * @returns true on success, false otherwise.
         */
        bool set_timeout(std::string str);

        /**
         * @brief Validates correctness of given address+port number and stores it into
         * appropriate attribute.
         * @param curr Current position in vector with options.
         * @param options Vector with options to parse.
         * @returns true on success, false otherwise.
         */
        bool set_address_port(size_t &curr, std::vector<std::string> options);


        /**
         * @brief Check validity of given request type.
         * @returns true on success, false otherwise.
         */
        bool check_req_type(request_type_t option);

        /**
         * @brief Handles parsing of options which requires arguments.
         * @param curr Current position in vector with options.
         * @param options Vector with options to parse.
         * @returns true on success, false otherwise.
         */
        bool require_arg(size_t &curr, std::vector<std::string> options);


        /**
         * @brief Handles parsing of -a option for case
         * when arguments are in format - ADDRESS,PORT
         * @returns true on success, false otherwise.
         */
        bool parse_addr_with_port(std::string str);

        /**
         * @brief Handles parsing of -a option for case
         * when arguments are in format - ADDRESS, PORT
         * @returns true on success, false otherwise.
         */
        bool parse_addr_with_sep(size_t curr, std::vector<std::string> options);

        /**
         * @brief Handles parsing of -a option for case
         * when arguments are in format - ADDRESS ,PORT
         * @returns true on success, false otherwise.
         */
        bool parse_port_with_sep(size_t curr, std::vector<std::string> options);

        /**
         * @brief Handles parsing of -a option for case
         * when arguments are in format - ADDRESS , PORT
         * @returns true on success, false otherwise.
         */
        bool parse_addr_with_spaces(size_t &curr, std::vector<std::string> options);
};

#endif