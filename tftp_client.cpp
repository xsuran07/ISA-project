#include <iostream>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <iomanip>
#include <ctime>
#include <chrono>

#include "tftp_client.h"

#define TIMOUT 5

static void print_timestamp()
{
    auto t = std::chrono::system_clock::now();

    //get number of milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()) % 1000;

    auto curr_time = std::chrono::system_clock::to_time_t(t);
    std::tm tm = *std::localtime(&curr_time);

    std::cout << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S.");
    std::cout << std::setfill('0') << std::setw(3) << ms.count() << "] ";
}

static void ipv4_tostring(struct sockaddr_in *addr, std::string &s)
{
    char buf[INET_ADDRSTRLEN];
    uint16_t port;

    inet_ntop(AF_INET, &addr->sin_addr.s_addr, buf, INET_ADDRSTRLEN);
    port = htons(addr->sin_port);

    s += buf;
    s += ":";
    s += std::to_string(port);
}

static void ipv6_tostring(struct sockaddr_in6 *addr, std::string &s)
{
    char buf[INET6_ADDRSTRLEN];
    uint16_t port;

    inet_ntop(AF_INET6, &addr->sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
    port = htons(addr->sin6_port);

    s += buf;
    s += ":";
    s += std::to_string(port);
}
void Tftp_client::logging(opcode_t type, bool sending)
{
    std::string str;

    print_timestamp();

    str += (sending)? "Sent " : "Recieved ";

    switch(type) {
        case OPCODE_ACK:
            str += "ACK ";
            break;
        case OPCODE_DATA:
            str += "DATA ";
            break;
        case OPCODE_RRQ:
            str += "RRQ ";
            break;
        case OPCODE_WRQ:
            str += "WRQ ";
            break;
        default:
            str += "ERROR ";
            break;
    }

    str += "packet ";
    str += (sending)? "to " : "from ";

    if(this->addr.ss_family == AF_INET) {
        ipv4_tostring((struct sockaddr_in *) &this->addr, str);
    } else {
        ipv6_tostring((struct sockaddr_in6 *) &this->addr, str);
    }

    std::cout << str;
    if(!this->log.empty()) {
        std::cout << " - " << this->log;
    }

    std::cout << std::endl;
}

bool Tftp_client::set_ipv4(Tftp_parameters *params)
{
    struct sockaddr_in *addr = (struct sockaddr_in *) &this->addr;

    if(inet_pton(AF_INET, params->get_address().c_str(), &addr->sin_addr.s_addr) <= 0) {
        return false;
    }

    this->addr_len = sizeof(struct sockaddr_in);
    addr->sin_port = htons(params->get_port());
    return true;
}

bool Tftp_client::set_ipv6(Tftp_parameters *params)
{
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &this->addr;

    if(inet_pton(AF_INET6, params->get_address().c_str(), &addr->sin6_addr.s6_addr) <= 0) {
        return false;
    }

    this->addr_len = sizeof(struct sockaddr_in6);
    addr->sin6_port = htons(params->get_port());
    return true;
}

Tftp_client::Tftp_client() : out_buffer(new uint8_t[MAX_SIZE]), in_buffer(new uint8_t[MAX_SIZE])
{
    this->size = MAX_SIZE;
    this->send_type = OPCODE_INVALID;

    memset(static_cast<void *> (this->out_buffer.get()), 0, MAX_SIZE);
    memset(static_cast<void *> (this->in_buffer.get()), 0, MAX_SIZE);
}

bool Tftp_client::handle_exchange(Tftp_parameters *params)
{
    bool ok = true;
    uint16_t resp_type;
    this->log.clear();

    // fill packet to send according to setting
    switch(this->send_type) {
    case OPCODE_RRQ:
        ok = fill_RRQ(params->get_filename().c_str());
        std::cout << "RRQ\n";
        break;
    case OPCODE_WRQ:
        ok = fill_WRQ(params->get_filename().c_str());
        std::cout << "WRQ\n";
        break;
    case OPCODE_DATA:
        ok = fill_DATA();
        std::cout << "DATA\n";
        break;
    case OPCODE_ACK:
        ok = fill_ACK();
        std::cout << "ACK\n";
        break;
    default:
        std::cerr << "Invalid type of packet!" << std::endl;
        return false;
    }

    if(!ok) {
        std::cerr << "Packet filling failed!" << std::endl;
        return false;
    }

    // send packet
    if(!send_packet()) {
        return false;
    }

    this->logging(this->send_type, true);

    if(!this->exp_resp) {
        this->last = true;
        return true;
    }


    this->log.clear();
    
    // wait for packet
    if(!recv_packet()) {
        return false;
    }

    std::cout << "LEN: " << this->resp_len << std::endl;

    if(!read_type(resp_type)) {
        return false;
    }

    switch(resp_type) {
    case OPCODE_ERROR:
        parse_ERROR();
        ok = false;
        break;
    case OPCODE_DATA:
        ok = parse_DATA();
        break;
    case OPCODE_ACK:
        std::cout << "GOT ACK!\n";
        ok = true;
        break;
    default:
        std::cerr << "Got unknown type of tftp packet!" << std::endl;
        return false;
    }

    if(resp_type == OPCODE_ERROR || ok) {
        this->logging(static_cast<opcode_t> (resp_type), false);
    }

    return ok;
}


bool Tftp_client::check_address_ipv4(struct sockaddr_in *addr)
{
    char buf[INET_ADDRSTRLEN];
    struct sockaddr_in *origin = (struct sockaddr_in *) &this->addr;
    
    inet_ntop(AF_INET, &addr->sin_addr.s_addr, buf, INET_ADDRSTRLEN);
    std::cout << "Address: " << buf << std::endl;
    std::cout << "PORT: " << ntohs(addr->sin_port) << std::endl;
  
    if(addr->sin_addr.s_addr != origin->sin_addr.s_addr) {
        std::cerr << "Packet from unexpected source (ipv4)!" << std::endl;
        return false;
    }

    if(this->first) {
        this->first = false;
        origin->sin_port = addr->sin_port;
        return true;
    } else {
        return addr->sin_port == origin->sin_port;
    }
}

bool Tftp_client::check_address_ipv6(struct sockaddr_in6 *addr)
{
    char buf[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *origin = (struct sockaddr_in6 *) &this->addr;

    inet_ntop(AF_INET, &addr->sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
    std::cout << "Address: " << buf << std::endl;
    std::cout << "PORT: " << ntohs(addr->sin6_port) << std::endl;
    for(int i = 0; i < 16; i++) {
        if(origin->sin6_addr.s6_addr[i] != addr->sin6_addr.s6_addr[i]) {
            std::cerr << "Packet from unexpected source (ipv6)!" << std::endl;
            return false;
        }
    }
    
    if(this->first) {
        this->first = false;
        origin->sin6_port = addr->sin6_port;
        return true;
    } else {
        return addr->sin6_port == origin->sin6_port;
    }
}

bool Tftp_client::check_address(struct sockaddr_storage *addr)
{
    if(this->addr.ss_family == AF_INET) {
        return check_address_ipv4((struct sockaddr_in *) addr);
    } else {
        return check_address_ipv6((struct sockaddr_in6 *) addr);
    }
}

int Tftp_client::recvfrom_wrapper(struct sockaddr_storage *src_addr, socklen_t *size)
{
    return recvfrom(
        this->sock,
        static_cast<void *> (this->in_buffer.get()),
        this->size,
        0,
        (struct sockaddr *) src_addr,
        size
    );
}

bool Tftp_client::recv_packet()
{
    struct sockaddr_storage src_addr;
    socklen_t size = sizeof(struct sockaddr_storage);
    ssize_t ret;
    bool first = true;

    while(1) {
        ret = recvfrom_wrapper(&src_addr, &size);

        if(ret > 0) { // successfully recieved some data
            break;
        } else if(first) { // timout expired first time => resend
            first = false;
            print_timestamp();
            std::cout << "Timout expired - re-sending last packet!" << std::endl;
            if(!send_packet()) {
                return false;
            }
        } else { // timeot expired second time => fail
            print_timestamp();
            std::cout << "Timout expired again!" << std::endl;
            return false;
        }
    }

    if(!check_address(&src_addr)) {
        return false;
    }

    this->resp_len = ret;
    return true;
}

bool Tftp_client::create_socket()
{
    struct timeval timout = {TIMOUT, 0};

    this->sock = socket(this->addr.ss_family, SOCK_DGRAM, 0);

    if(sock == -1) {
        std::cerr << "socket() failed!" << std::endl;
        return false;
    }

    // set timout for recieving
    if(setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &timout, sizeof(struct timeval)) < 0) {
        close(this->sock);
        return false;
    }

    return true;
}

bool Tftp_client::communicate(Tftp_parameters *params)
{
    bool ok = true;
    this->send_type = (params->get_req_type() == Tftp_parameters::READ) ? OPCODE_RRQ : OPCODE_WRQ;
    this->first = true;
    this->exp_resp = true;
    this->last = false;

    if(!process_address(params)) {
        return false;
    }

    if(!create_socket()) {
        return false;
    }

    do {
        ok = handle_exchange(params);
    } while(ok && !this->last);

    print_timestamp();
    if(ok) {
        std::cout << "Transfer completed without errors." << std::endl;
    } else {
        std::cout << "Transfer didn't complete sucessfully!" << std::endl;
    }

    close(this->sock);
    return true;
}



bool Tftp_client::process_address(Tftp_parameters *params)
{
    this->addr.ss_family = params->get_addr_family();

    if(this->addr.ss_family == AF_INET) {
        return set_ipv4(params);
    } else {
        return set_ipv6(params);
    }
}

bool Tftp_client::send_packet()
{
    ssize_t ret;
    
    ret = sendto(
        this->sock,
        static_cast<void *> (this->out_buffer.get()),
        this->out_curr_pos,
        0,
        (struct sockaddr *) &this->addr,
        this->addr_len
    );

    if(ret == -1) {
        std::cerr << "sendto() failed!" << std::endl;
        return false;
    }

    return true;
}

bool Tftp_client::write_byte(uint8_t b)
{
    if(this->out_curr_pos >= this->size) {
        return false;
    }

    this->out_buffer[this->out_curr_pos] = b;
    this->out_curr_pos++;
    return true;
}

bool Tftp_client::write_word(uint16_t w)
{
    uint16_t converted;

    if(this->out_curr_pos + 1 >= this->size) {
        return false;
    }

    converted = htons(w);
    memcpy(&this->out_buffer[this->out_curr_pos], &converted, 2);

    this->out_curr_pos += 2;
    return true;
}

bool Tftp_client::write_string(const char *str)
{
    size_t len = strlen(str);

    if(this->out_curr_pos + len >= this->size) {
        return false;
    }

    for(size_t i = 0; i < len; i++) {
        if(!write_byte(str[i])) {
            return false;
        }

    }

    return write_byte('\0');
}

bool Tftp_client::fill_RQ(std::string filename, opcode_t opcode)
{
    this->out_curr_pos = 0; // reinitialize

    do {
        //OPCODE
        if(!write_word(opcode)) {
            break;
        }

        //FILENAME
        if(!write_string(filename.c_str())) {
            break;
        }

        //MODE
        if(!write_string("octet")) {
            break;
        }

        std::cout << "PACKET filled!" << std::endl;
        return true;
    } while(0);

    std::cerr << "Error while creating RRQ/WRQ packet" << std::endl;
    return false;
}

bool Tftp_client::fill_RRQ(std::string filename)
{
    this->block_num = 1;
    this->exp_type = OPCODE_DATA;

    return fill_RQ(filename, OPCODE_RRQ);
}

bool Tftp_client::fill_WRQ(std::string filename)
{
    this->block_num = 0;
    this->exp_type = OPCODE_ACK;

    return fill_RQ(filename, OPCODE_WRQ);
}

bool Tftp_client::fill_ACK()
{
    this->out_curr_pos = 0; // reinitialize
    this->exp_type = OPCODE_DATA;

    do {
        if(!write_word(OPCODE_ACK)) {
            break;
        }

        if(!write_word(this->block_num)) {
            break;
        }

        this->block_num++;
        return true;
    } while(0);

    std::cerr << "Error while creating  packet" << std::endl;
    return false;
}

bool Tftp_client::fill_DATA()
{
    return true;
}

bool Tftp_client::read_type(uint16_t &res)
{
    this->in_curr_pos = 0;

    return read_word(res);
}

bool Tftp_client::read_byte(uint8_t &res)
{
    if(this->in_curr_pos >= this->resp_len) {
        return false;
    }

    res = this->in_buffer[this->in_curr_pos];
    this->in_curr_pos++;
    return true;
}

bool Tftp_client::read_word(uint16_t &res)
{
    uint16_t tmp;

    if(this->in_curr_pos + 1 >= this->resp_len) {
        return false;
    }

    tmp = *((uint16_t *) &this->in_buffer[this->in_curr_pos]);
    res = ntohs(tmp);

    this->in_curr_pos += 2;
    return true;
}

bool Tftp_client::read_string(std::string &res)
{
    uint8_t c;
    res.clear();

    while(true) {
        if(!read_byte(c)) {
            return false;
     }

        if(c == '\0') {
            break;
        }
        res.push_back(c);
    }

    return true;
}

bool Tftp_client::parse_ERROR()
{
    uint16_t err_code;
    std::string err_msg;

    do {
        if(!read_word(err_code)) {
            break;
        }

        if(!read_string(err_msg)) {
            break;
        }

        if(this->in_curr_pos != this->resp_len) {
            std::cerr << "Invalid ERROR packet format!" << std::endl;
            return false;
        }

        this->log += "code: " + std::to_string(err_code) + ", msg: " + err_msg;
        return true;
    } while(0);

    std::cerr << "Error while parsing ERROR packet!" << std::endl;
    return false;
}

bool Tftp_client::parse_ACK()
{
    uint16_t block_num;

    if(this->exp_type != OPCODE_ACK) {
        return false;
    }

    if(!read_word(block_num)) {
        std::cerr << "Error while parsing ACK packet!" << std::endl;
        return false;
    }


    if(this->in_curr_pos != this->resp_len) {
        std::cerr << "Invalid ACK packet format!" << std::endl;
        return false;
    }

    if(block_num != this->block_num) {
        return false;
    }

    this->log += "block number " + std::to_string(block_num);

    this->send_type = OPCODE_DATA;
    this->block_num++;
    return true;
}

bool Tftp_client::parse_DATA()
{
    uint16_t block_num;
    uint8_t c;

    if(this->exp_type != OPCODE_DATA) {
        return false;
    }

    if(!read_word(block_num))


    std::cout << "TFTP DATA - block: " << block_num << std::endl;

    std::cout << "----------------------------------\n";
    while(this->in_curr_pos < this->resp_len) {
        if(!read_byte(c)) {
            return false;
        }

        std::cout << c;
    }
    std::cout << "----------------------------------\n";

    if(this->resp_len - 4 < this->block_size) {
        std::cout << "LAST\n";
        this->exp_resp = false;
    }

    this->log += "block number " + std::to_string(block_num) + ", ";
    this->log += std::to_string(this->resp_len - 4) + " bytes";

    this->send_type = OPCODE_ACK;
    return true;
}