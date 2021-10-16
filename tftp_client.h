#ifndef __TFTP_CLIENT_H_
#define __TFTP_CLIENT_H_

#include <string>
#include <stdint.h>
#include <memory>
#include <sys/socket.h>
#include <fstream>

#include "tftp_parameters.h"

#define MAX_SIZE 1024

class Tftp_client
{
    public:
        typedef enum {
            OPCODE_INVALID,
            OPCODE_RRQ,
            OPCODE_WRQ,
            OPCODE_DATA,
            OPCODE_ACK,
            OPCODE_ERROR,
            OPCODE_SKIP,
        } opcode_t;

    private:
        std::fstream file;

        std::unique_ptr<uint8_t[]> out_buffer;
        std::unique_ptr<uint8_t[]> in_buffer;
        uint64_t out_curr_pos;
        uint64_t in_curr_pos;
        uint64_t resp_len;
        std::string log;

        bool last;
        bool exp_resp;
        uint64_t block_size;
        uint16_t block_num;
        opcode_t exp_type;
        opcode_t send_type;

        struct sockaddr_storage addr;
        size_t addr_len;

        uint64_t size;

        int sock;
        bool first;

    public:
        /**
         * @brief Constructor.
         */
        Tftp_client();

        bool communicate(Tftp_parameters *params);

    private:
        void logging(opcode_t type, bool sending);
        void cleanup();

        bool set_ipv4(Tftp_parameters *params);
        bool set_ipv6(Tftp_parameters *params);
        bool process_address(Tftp_parameters *params);
        bool create_socket();
        bool prepare_file(Tftp_parameters *params);

        bool handle_exchange(Tftp_parameters *params);
        bool send_packet();
        int recvfrom_wrapper(struct sockaddr_storage *src_addr, socklen_t *size);
        bool recv_packet();
        bool check_address(struct sockaddr_storage *addr);
        bool check_address_ipv4(struct sockaddr_in *addr);
        bool check_address_ipv6(struct sockaddr_in6 *addr);

        bool fill_RQ(std::string filename, opcode_t opcode);
        bool fill_RRQ(std::string filename);
        bool fill_WRQ(std::string filename);
        bool fill_DATA();
        bool fill_ACK();

        bool write_byte(uint8_t b);
        bool write_word(uint16_t w);
        bool write_string(const char *str);

        bool read_type(uint16_t &res);
        bool read_byte(uint8_t &res);
        bool read_word(uint16_t &res);
        bool read_string(std::string &res);

        bool parse_ERROR();
        bool parse_ACK();
        bool parse_DATA();
};

#endif