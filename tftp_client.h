/*
 * @author Jakub Šuráň (xsuran07)
 * @file tftp_client.h
 * @brief Interface of tftp_client class.
 */

#ifndef __TFTP_CLIENT_H_
#define __TFTP_CLIENT_H_

#include <string>
#include <stdint.h>
#include <memory>
#include <sys/socket.h>
#include <fstream>
#include <map>

#include "tftp_parameters.h"

#define MAX_SIZE 1024

/**
 * @brief Class representing TFTP client. It is able
 * to communicate with server according to given parameters.
 */
class Tftp_client
{
    public:
        /**
         * @brief Types of TFTP packet opcodes + 
         * some special types (INVALID and SKIP) to correctly
         * handle commutication.
         */
        typedef enum {
            OPCODE_INVALID,
            OPCODE_RRQ,
            OPCODE_WRQ,
            OPCODE_DATA,
            OPCODE_ACK,
            OPCODE_ERROR,
            OPCODE_OACK,
            OPCODE_SKIP,
        } opcode_t;

    typedef enum {
        ERR_CODE_NOT_DEF,
        ERR_CODE_NOT_FOUND,
        ERR_CODE_ACCESS_VIOLATION,
        ERR_CODE_DISK_FULL,
        ERR_CODE_ILEGAL_OP,
        ERR_CODE_UNKNOWN_ID,
        ERR_CODE_FILE_AXISTS,
        ERR_CODE_NO_SUCH_USER,
        ERR_CODE_PROBLEMATIC_OPTION,
    } err_code_t;

    private:
        std::fstream file;
        int sock;

        std::unique_ptr<uint8_t[]> out_buffer;
        std::unique_ptr<uint8_t[]> in_buffer;
        uint64_t out_curr_pos;
        uint64_t in_curr_pos;
        uint64_t resp_len;
        std::string log;

        std::map<std::string, std::string> options;
        bool last;
        bool exp_resp;
        uint64_t block_size;
        uint16_t block_num;
        opcode_t exp_type;
        opcode_t send_type;
        bool binary;
        bool active_cr;
        std::string bytes_left;
        uint64_t cur_size;
        uint64_t tsize;
        err_code_t error_code;
        uint16_t original_TID;
        bool resend_rq;

        struct sockaddr_storage addr;
        size_t addr_len;

        uint64_t size;

        bool first;

    public:
        /**
         * @brief Constructor.
         */
        Tftp_client();

        /**
         * @brief Handles communication with server. This includes preparation
         * of all necessary components according to given parameters + 
         * controlling communication itself.
         * @param params Structure with TFTP parameters for this communication - specifies
         * type of request, file to get/put, etc.
         * @returns true if communication was succesful, false otherwise.
         */
        bool communicate(Tftp_parameters *params);

        /**
         * @brief Static method. Prints current timestamp in format
         * YYYY-mm-dd HH:MM:SS.ms.
         */
        static void print_timestamp();

        /**
         * @brief Converts given ipv4 address + port into string representation.
         * @param addr Sockaddr_in structure with ipv4 address to convert.
         * @param s String to store result into.
         */
        static void ipv4_tostring(struct sockaddr_in *addr, std::string &s);

        /**
         * @brief Converts given ipv6 address + port into string representation.
         * @param addr Sockaddr_in6 structure with ipv6 address to convert.
         * @param s String to store result into.
         */
        static void ipv6_tostring(struct sockaddr_in6 *addr, std::string &s);

    private:
        /**
         * @brief Parses, builds and prints log message from
         * last operetion.
         * @param type Type of last send packet.
         * @param sending Determines if last operation has been sending
         * or not.
         */
        void logging(opcode_t type, bool sending);

        /**
         * @brief Release all sources - close socket, files, etc.
         */
        void cleanup();


        /**
         * @brief Sets initial value to some atributes before start of
         * communication.
         */
        void init(Tftp_parameters *params);

        /**
         * @brief Extracts ipv4 address + port from given parameters and
         * correctly set them into attributes.
         * @param params Structure with TFTP parameters to extract
         * information from.
         * @returns true in case of success, false otherwise.
         */
        bool set_ipv4(Tftp_parameters *params);

        /**
         * @brief Extracts ipv6 address + port from given parameters and
         * correctly set them into attributes.
         * @param params Structure with TFTP parameters to extract
         * information from.
         * @returns true in case of success, false otherwise.
         */
        bool set_ipv6(Tftp_parameters *params);

        /**
         * @brief Extracts server's address + port from given
         * parameters and stores them into appropriate attributes.
         * Manage both IPV4 and IPV4 addresses.
         * @param params Structure with TFTP parameters to extract
         * information from.
         * @returns true in case of success, false otherwise.
         */
        bool process_address(Tftp_parameters *params);

        /**
         * @brief Creates sokcet for communication. Socket's
         * parameters are taken from attributes.
         * @returns true in case of success, false otherwise.
         */
        bool create_socket();

        /**
         * @brief According to supplied parameters, opens file
         * in desired mode.
         * @param params Structure with TFTP parameters to take
         * necessary information from.
         * @returns true in case of success, false otherwise.
         */
        bool prepare_file(Tftp_parameters *params);

        void get_filesize();

        void set_options(Tftp_parameters *params);


        bool handle_exchange(Tftp_parameters *params);
        bool send_packet();
        int recvfrom_wrapper(struct sockaddr_storage *src_addr, socklen_t *size);
        bool recv_packet();
        bool check_address(struct sockaddr_storage *addr);
        bool check_address_ipv4(struct sockaddr_in *addr);
        bool check_address_ipv6(struct sockaddr_in6 *addr);
        void reset_TID();
        void reset_ipv4_TID();
        void reset_ipv6_TID();


        /**
         * @brief Try to fill appropriate data into request packets (RRQ or WRQ)
         * accoring to parameter.
         * @param filename Determines name of file to include into request.
         * @param opcode Determines, which type of request shall be used
         * (RRQ or WRQ).
         * @returns true in case of success, false otherwise.
         */
        bool fill_RQ(std::string filename, opcode_t opcode);

        /**
         * @brief Try to fill appropriate data into RRQ packet.
         * @param filename Determines name of file to include into request.
         * @returns true in case of success, false otherwise.
         */
        bool fill_RRQ(std::string filename);

        /**
         * @brief Try to fill appropriate data into WRQ packet.
         * @param filename Determines name of file to include into request.
         * @returns true in case of success, false otherwise.
         */
        bool fill_WRQ(std::string filename);

        /**
         * @brief Try to fill appropriate data into DATA packet.
         * @returns true in case of success, false otherwise.
         */
        bool fill_DATA();
        
        /**
         * @brief Try to fill appropriate data into ACK packet.
         * @returns true in case of success, false otherwise.
         */
        bool fill_ACK();

        bool fill_ERROR();

        bool write_two_bytes(uint8_t c1, uint8_t c2);
        bool write_byte(uint8_t b);
        bool write_word(uint16_t w);
        bool write_string(const char *str);
        bool write_options();

        bool read_type(uint16_t &res);
        bool read_cr(uint8_t &res);
        bool read_byte(uint8_t &res);
        bool read_word(uint16_t &res);
        bool read_string(std::string &res);


        /**
         * @brief Try to parse and extract information from
         * recieved ERROR packet.
         * @returns true in case of success, false otherwise.
         */
        bool parse_ERROR();

        /**
         * @brief Try to parse and extract information from
         * recieved ACK packet.
         * @returns true in case of success, false otherwise.
         */
        bool parse_ACK();

        /**
         * @brief Try to parse and extract information from
         * recieved DATA packet.
         * @returns true in case of success, false otherwise.
         */
        bool parse_DATA();

        bool parse_OACK();

        bool validate_option(std::string option, std::string value);

        bool check_packet_type(uint16_t resp_type);
};

#endif