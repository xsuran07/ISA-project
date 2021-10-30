/*
 * @author Jakub Šuráň (xsuran07)
 * @file tftp_client.cpp
 * @brief Implementation of tftp_client class.
 */

#include <iostream>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>

#include "tftp_client.h"

#define TIMEOUT 5
#define HARD_TIMEOUT (3*TIMEOUT + 1)
#define UDP_HEADER 8
#define TFTP_HEADER 4
#define MAX_IP_HEADER 60
#define MIN_BLOCK_SIZE 8
// #define DEBUG

// STATIC METHODS

void Tftp_client::print_timestamp()
{
    auto t = std::chrono::system_clock::now();

    //get number of milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()) % 1000;

    auto curr_time = std::chrono::system_clock::to_time_t(t);
    std::tm tm = *std::localtime(&curr_time);

    std::cout << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S.");
    std::cout << std::setfill('0') << std::setw(3) << ms.count() << "] ";
}

void Tftp_client::ipv4_tostring(struct sockaddr_in *addr, std::string &s)
{
    char buf[INET_ADDRSTRLEN];
    uint16_t port;

    // get string representation of ipv4 address + port
    inet_ntop(AF_INET, &addr->sin_addr.s_addr, buf, INET_ADDRSTRLEN);
    port = htons(addr->sin_port);

    s += buf;
    s += ":";
    s += std::to_string(port);
}

void Tftp_client::ipv6_tostring(struct sockaddr_in6 *addr, std::string &s)
{
    char buf[INET6_ADDRSTRLEN];
    uint16_t port;

    // get string representation of ipv6 address + port
    inet_ntop(AF_INET6, &addr->sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
    port = htons(addr->sin6_port);

    s += "[";
    s += buf;
    s += "]:";
    s += std::to_string(port);
}

uint8_t *Tftp_client::resize(uint8_t *old_buf, uint64_t new_size)
{
    uint8_t *new_buf;

    // frees old buffer
    if(old_buf != nullptr) {
        delete[] old_buf;
    }

    new_buf = new(std::nothrow) uint8_t[new_size];

    // set initial values
    if(new_buf != nullptr) {
        memset(new_buf, 0, new_size);
    }

    return new_buf;
}

// PUBLIC INSTANCE METHODS

// contstructor
Tftp_client::Tftp_client() : out_buffer(new uint8_t[MAX_SIZE]), in_buffer(new uint8_t[MAX_SIZE])
{
    this->size = MAX_SIZE;
    this->send_type = OPCODE_INVALID;

    memset(static_cast<void *> (this->out_buffer.get()), 0, MAX_SIZE);
    memset(static_cast<void *> (this->in_buffer.get()), 0, MAX_SIZE);
}

// handles communication with server
bool Tftp_client::communicate(Tftp_parameters *params)
{
    bool ok = true;

    init(params);

    // set extension options values
    set_options(params);

    // process and store address of the server
    if(!process_address(params)) {
        return false;
    }

    // open and configure socket for communication
    if(!create_socket()) {
        return false;
    }

    // check if proposed block isze can be satisfy with available MTU
    if(!check_max_blksize(params->get_size())) {
        close(this->sock);
        return false;
    }

    // try to open specified file
    if(!prepare_file(params)) {
        close(this->sock);
        return false;
    }

    // communicate with server till error or successful transfer
    do {
        ok = handle_exchange(params);
    } while(ok && !this->last);

    // report result of transfer
    print_timestamp();
    if(ok) {
        std::cout << "Transfer completed without errors." << std::endl;
    } else {
        std::cout << "Transfer didn't complete sucessfully!" << std::endl;
    }

    cleanup();
    return true;
}

// GENERAL PRIVATE INSTANCE METHODS

void Tftp_client::logging(opcode_t type, bool sending)
{
    std::string str;

    if(sending && this->resend_rq) {
        str += "Re-sent ";
    } else {
        str += (sending)? "Sent " : "Recieved ";
    }

    switch(type) {
        case OPCODE_SKIP:
            return;
        case OPCODE_ACK:
            str += "ACK ";
            break;
        case OPCODE_DATA:
            str += "DATA ";
            if(this->binary) {
                this->log += "(total " + std::to_string(this->cur_size) + "/" + std::to_string(this->tsize) + ")";
            }
            break;
        case OPCODE_RRQ:
            str += "RRQ ";
            break;
        case OPCODE_WRQ:
            str += "WRQ ";
            break;
        case OPCODE_OACK:
            str += "OACK ";
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

    if(sending && this->resend_rq) {
        str += " without options";
        this->resend_rq = false;
    }

    print_timestamp();
    std::cout << str;
    if(!this->log.empty()) {
        std::cout << " - " << this->log;
    }

    std::cout << std::endl;
}

void Tftp_client::cleanup()
{
    close(this->sock);
    this->file.close();
}

// PRIVATE INSTANCE METHODS FOR NECCESSARY PREPARATION BEFORE COMMUNICATION

void Tftp_client::init(Tftp_parameters *params)
{
    this->binary = params->get_mode() == Tftp_parameters::BINARY;
    this->send_type = (params->get_req_type() == Tftp_parameters::READ) ? OPCODE_RRQ : OPCODE_WRQ;
    this->first = true;
    this->exp_resp = true;
    this->last = false;
    this->bytes_left.clear();
    this->block_size = 512;
    this->cur_size = 0;
    this->tsize = 0;
    this->original_TID = htons(params->get_port());
    this->resend_rq = false;
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
    addr->sin6_scope_id = 0;
    addr->sin6_flowinfo = 0;
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

bool Tftp_client::create_socket()
{
    struct timeval timout = {TIMEOUT, 0};

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

bool Tftp_client::prepare_file(Tftp_parameters *params)
{
    std::vector<std::string> parts;
    Tftp_parameters::split_string(params->get_filename(), "/", parts);
    std::string name_of_file = parts[parts.size() - 1];
    std::ios::openmode mode = std::fstream::binary;

    if(params->get_req_type() == Tftp_parameters::READ) {
        mode |= std::fstream::out;
    } else {
        mode |= std::fstream::in;
    }

    this->file.open(name_of_file, mode);
    if(this->file.fail()) {
        std::cerr << "Opening of file \"" << name_of_file << "\"failed!" << std::endl;
        return false;
    }

    return true;
}

void Tftp_client::get_filesize()
{
    this->file.seekg(0, this->file.end);
    this->tsize = this->file.tellg();
    this->file.seekg(0, this->file.beg);

#ifdef DEBUG
    std::cout << "fds: " << this->tsize << "\n";
#endif
}

void Tftp_client::set_options(Tftp_parameters *params)
{
    this->options.clear();

    // get filesize when writing to server
    if(params->get_req_type() == Tftp_parameters::WRITE) {
        get_filesize();
    }

    // for binary mode include tszie extension into packet
    if(this->binary) {
        this->options["tsize"] = std::to_string(this->tsize);
    }

    // if requested, set option timeout value
    if(params->get_timeout() > 0) {
        this->options["timeout"] = std::to_string(params->get_timeout());
    }

    // set option blksize value for nondefault vaules
    if(params->get_size() != 512) {
        this->options["blksize"] = std::to_string(params->get_size());
    }
}

bool Tftp_client::check_max_blksize(int block_size)
{
    struct ifaddrs *addrs;
    struct ifreq ifr;
    int min_mtu = -1;
    const int headers = MAX_IP_HEADER + UDP_HEADER + TFTP_HEADER;

    getifaddrs(&addrs);

    // check all interefaces
    for(struct ifaddrs *a = addrs; a != NULL; a = a->ifa_next) {
        // we're interested only in address family same as our socket
        if(a->ifa_addr->sa_family != this->addr.ss_family) {
            continue;
        }

        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, a->ifa_name, sizeof(ifr.ifr_name));

        // get information about interface
        if(ioctl(this->sock, SIOCGIFMTU, &ifr) != 0 || ifr.ifr_mtu < 0) {
            continue;
        }

        // first assingment
        if(min_mtu < 0) {
            min_mtu = ifr.ifr_mtu;
        } else {
            min_mtu = (min_mtu > ifr.ifr_mtu)? ifr.ifr_mtu : min_mtu;
        }
    }

    // make sure smallest mtu is big enough
    if(min_mtu < headers + MIN_BLOCK_SIZE) {
        std::cerr << "Not able find interface with MTU large enough" << std::endl;
        return false;
    }

    min_mtu = min_mtu - headers;
    std::cout << "MIn: " << min_mtu << std::endl;

    if(block_size > min_mtu) {
        this->options["blksize"] = std::to_string(min_mtu);

        std::cout << "Warning! Proposed blocksize (" << block_size
            << ") is too big! Value " << min_mtu << " will be used (based on available MTUs)." << std::endl;
    }

    freeifaddrs(addrs);
    return true;
}

// PRIVATE INSTANCE METHODS TO HADNLE COMMUNICATION ITSELF

bool Tftp_client::handle_exchange(Tftp_parameters *params)
{
    time_t curr_time;
    bool ok = true;
    bool skip = false;
    uint16_t resp_type;
    this->log.clear();

    // fill packet to send according to setting
    switch(this->send_type) {
    case OPCODE_SKIP:
        skip = true;
        break;
    case OPCODE_RRQ:
        ok = fill_RRQ(params->get_filename().c_str());
        break;
    case OPCODE_WRQ:
        ok = fill_WRQ(params->get_filename().c_str());
        break;
    case OPCODE_DATA:
        ok = fill_DATA();
        break;
    case OPCODE_ACK:
        ok = fill_ACK();
        break;
    default:
        send_ERROR(ERR_CODE_NOT_DEF, "Internal error!");
        std::cerr << "Cannot send this type of packet!" << std::endl;
        return false;
    }

    if(!ok) {
        send_ERROR(ERR_CODE_NOT_DEF, "Internal error while filling packet!");
        std::cerr << "Packet filling failed!" << std::endl;
        return false;
    }

    // send packet
    if(!skip && !send_packet()) {
        send_ERROR(ERR_CODE_NOT_DEF, "Internal error while sending packet!");
        return false;
    }

    if(!skip) {
        this->logging(this->send_type, true);
        curr_time = std::time(0);
        this->timer = curr_time + HARD_TIMEOUT;
        this->resend_timer = curr_time + TIMEOUT;
    }

    if(!this->exp_resp) {
        this->last = true;
        return true;
    }

    this->log.clear();
    
    // wait for packet
    if(!recv_packet()) {
        return false;
    }

    // extract type of recieved packet
    if(!read_type(resp_type)) {
        send_ERROR(ERR_CODE_NOT_DEF, "Internal error while!");
        return false;
    }

    // check packet's type
    if(!check_packet_type(resp_type)) {
        send_ERROR(ERR_CODE_ILEGAL_OP, "Invalid packet type!");
        return false;
    }

    switch(resp_type) {
    case OPCODE_ERROR:
        ok = parse_ERROR();
        break;
    case OPCODE_DATA:
        ok = parse_DATA();
        break;
    case OPCODE_ACK:
        ok = parse_ACK();
        break;
    case OPCODE_OACK:
        ok = parse_OACK();
        break;
    default:
        send_ERROR(ERR_CODE_NOT_DEF, "Internal error!");
        std::cerr << "Got unknown type of tftp packet!" << std::endl;
        return false;
    }

    if(ok) {
        this->logging(static_cast<opcode_t> (resp_type), false);
        ok = !(resp_type == OPCODE_ERROR && this->last); // ERROR as last packet of communication means unsuccess
    } else {
        send_ERROR(ERR_CODE_ILEGAL_OP, "Invalid packet!");
        return false;
    }

    return ok;
}

bool Tftp_client::check_address_ipv4(struct sockaddr_in *addr)
{
    char buf[INET_ADDRSTRLEN];
    struct sockaddr_in *origin = (struct sockaddr_in *) &this->addr;
    
    inet_ntop(AF_INET, &addr->sin_addr.s_addr, buf, INET_ADDRSTRLEN);
#ifdef DEBUG
    std::cout << "Address: " << buf << std::endl;
    std::cout << "PORT: " << ntohs(addr->sin_port) << std::endl;
#endif

    do {
        //check address
        if(addr->sin_addr.s_addr != origin->sin_addr.s_addr) {
            break;
        }

        // in case of first response, get server's TID
        if(this->first) {
            this->first = false;
            origin->sin_port = addr->sin_port;
        // check correctness of server's TID
        } else if(addr->sin_port != origin->sin_port) {
            break;
        }

        return true;
    } while(0);

    // backup
    unsigned long orig_addr = origin->sin_addr.s_addr;
    unsigned short orig_port = origin->sin_port;
    uint8_t orig_buf[this->out_curr_pos];
    memcpy(orig_buf, static_cast<void *> (this->out_buffer.get()), this->out_curr_pos);

    // set temporary values
    origin->sin_addr.s_addr = addr->sin_addr.s_addr;
    origin->sin_port = addr->sin_port;

    std::cerr << "Got packet with unknown TID (IPV4)" << std::endl;
    send_ERROR(ERR_CODE_UNKNOWN_ID, "Unknown TID!");

    // restore original values
    origin->sin_addr.s_addr = orig_addr;
    origin->sin_port = orig_port;
    memcpy(static_cast<void *> (this->out_buffer.get()), orig_buf, this->out_curr_pos);

    return false;
}

bool Tftp_client::check_address_ipv6(struct sockaddr_in6 *addr)
{
    char buf[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *origin = (struct sockaddr_in6 *) &this->addr;
    bool ok = true;

    inet_ntop(AF_INET, &addr->sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
#ifdef DEBUG
    std::cout << "Address: " << buf << std::endl;
    std::cout << "PORT: " << ntohs(addr->sin6_port) << std::endl;
#endif
    do {
        for(int i = 0; i < 16; i++) {
            if(origin->sin6_addr.s6_addr[i] != addr->sin6_addr.s6_addr[i]) {
                std::cerr << "Packet from unexpected source (ipv6)!" << std::endl;
                ok = false;
                break;
            }
        }

        if(!ok) {
            break;
        }

        // in case of first response, get server's TID
        if(this->first) {
            this->first = false;
            origin->sin6_port = addr->sin6_port;
            return true;
        // check correctness of server's TID
        } else if(addr->sin6_port != origin->sin6_port) {
            break;
        }

        return true;
    } while(0);

    // backup
    unsigned char orig_addr[16];
    memcpy(orig_addr, origin->sin6_addr.s6_addr, 16);
    unsigned short orig_port = origin->sin6_port;
    uint8_t orig_buf[this->out_curr_pos];
    memcpy(orig_buf, static_cast<void *> (this->out_buffer.get()), this->out_curr_pos);

    // set temporary values
    origin->sin6_port = addr->sin6_port;
    memcpy(origin->sin6_addr.s6_addr, addr->sin6_addr.s6_addr, 16);

    std::cerr << "Got packet with unknown TID (IPV6)" << std::endl;
    send_ERROR(ERR_CODE_UNKNOWN_ID, "Unknown TID!");

    // restore original values
    memcpy(origin->sin6_addr.s6_addr, orig_addr, 16);
    origin->sin6_port = orig_port;
    memcpy(static_cast<void *> (this->out_buffer.get()), orig_buf, this->out_curr_pos);

    return false;
}

bool Tftp_client::check_address(struct sockaddr_storage *addr)
{
    bool ret;

    if(this->addr.ss_family == AF_INET) {
        ret = check_address_ipv4((struct sockaddr_in *) addr);
    } else {
        ret = check_address_ipv6((struct sockaddr_in6 *) addr);
    }

    if(!ret) {
        if(std::time(0) > this->resend_timer) {
            resend_last();
        }
    }

    return ret;
}

void Tftp_client::reset_ipv4_TID()
{
    struct sockaddr_in *addr = (struct sockaddr_in *) &this->addr;

    addr->sin_port = this->original_TID;
}

void Tftp_client::reset_ipv6_TID()
{
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &this->addr;

    addr->sin6_port = this->original_TID;
}

void Tftp_client::reset_TID()
{
    if(this->addr.ss_family == AF_INET) {
        reset_ipv4_TID();
    } else {
        reset_ipv6_TID();
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

bool Tftp_client::resend_last()
{
    print_timestamp();
    this->resend_timer = std::time(0) + TIMEOUT;
    std::cout << "Timout expired - re-sending last packet!" << std::endl;

    return send_packet();
}

bool Tftp_client::recv_packet()
{
    struct sockaddr_storage src_addr;
    socklen_t size = sizeof(struct sockaddr_storage);
    ssize_t ret;
    time_t curr_time = std::time(0);

    // in case resending timeout was interrupt with other packets
    if(curr_time > this->resend_timer) {
        if(!resend_last()) {
            return false;
        }
    }

    while(1) {
        ret = recvfrom_wrapper(&src_addr, &size);
        curr_time = std::time(0);

        if (curr_time > this->timer) {
            print_timestamp();
            std::cout << "Transfer time-out!" << std::endl;
            break;
        }

        if(ret > 0) { // successfully recieved some data
            if(check_address(&src_addr)) {
                this->resp_len = ret;
                return true;
            }
        } else { // timout expired => resend
            if(!resend_last()) {
                break;
            }
        }
    }

    return false;
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

bool Tftp_client::check_packet_type(uint16_t resp_type)
{
    std::vector<std::string> types({"none", "RRQ", "WRQ", "DATA", "ACK", "ERROR"});

    // error packet automaticaly matches everything
    if(resp_type == OPCODE_ERROR) {
        return true;
    }

    // recieved packet type match expected type
    if(resp_type == this->exp_type) {
        return true;
    }

    // duplicate OACK packet is acceptable
    if(resp_type == OPCODE_OACK) {
        return true;
    }

    if(this->exp_type == OPCODE_OACK) {
        // server ignores options and immediately sends data on RRQ
        if(this->send_type == OPCODE_RRQ && resp_type == OPCODE_DATA) {
            return true;
        // server ignores options and immediately sends ACK on WRQ
        } else if(this->send_type == OPCODE_WRQ && resp_type == OPCODE_ACK) {
            return true;
        }

    }

    print_timestamp();
    std::cout << "Recieved wrong type of packet! Expected " << types[this->exp_type] <<
        ", got " << types[resp_type];

    return false;
}

void Tftp_client::send_ERROR(err_code_t code, std::string msg)
{
    do {
        if(!fill_ERROR(code, msg)) {
            break;
        }

        if(!send_packet()) {
            break;
        }

        logging(OPCODE_ERROR, true);
    } while(0);
}

// PRIVATE INSTANCE METHODS TO SIMPLIFY FILLING OF DATA INTO PACKETS

bool Tftp_client::write_two_bytes(uint8_t c1, uint8_t c2)
{
    if(this->out_curr_pos + 1 >= this->size) {
        return false;
    }

    this->out_buffer[this->out_curr_pos] = c1;
    this->out_curr_pos++;

    // c2 byte fits into current data block
    if(this->out_curr_pos < this->block_size) {
        this->out_buffer[this->out_curr_pos] = c2;
        this->out_curr_pos++;
    // c2 byte will be part of next data block
    } else {
        this->bytes_left.push_back(c2);
    }

    this->active_cr = c2 == '\r';
    return true;
}

bool Tftp_client::write_byte(uint8_t b)
{
    if(this->out_curr_pos >= this->size) {
        return false;
    }

    if(!this->binary) {
        // netascii line ending
        if(!this->active_cr && b == '\n') {
            return write_two_bytes('\r', '\n');
        // CR has to be followed by \0
        } else if(this->active_cr && b != '\n') {
            return write_two_bytes('\0', b);
        }
    }

    this->active_cr = b == '\r';
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

bool Tftp_client::write_options()
{
    if(this->options.empty()) {
        return true;
    }

    log += ", options:";

    exp_type = OPCODE_OACK;

    for(auto it = this->options.begin(); it != this->options.end(); ++it) {
        log += (it == this->options.begin())? " " : ", ";
        log += it->first + "(" + it->second + ")";

        if(!write_string(it->first.c_str())) {
            std::cerr << "Error while writing option name - " << it->first << std::endl;
            return false;
        }

        if(!write_string(it->second.c_str())) {
            std::cerr << "Error while writing option value - " << it->second << std::endl;
            return false;
        }
    }

    return true;
}

// PRIVATE INSTANCE METHODS FOR FILLING TFPT PACKETS TO BE SENT

bool Tftp_client::fill_RQ(std::string filename, opcode_t opcode)
{
    std::string mode = (this->binary)? "octet" : "netascii";
    this->out_curr_pos = 0; // reinitialize
    this->active_cr = false;

    do {
        // OPCODE
        if(!write_word(opcode)) {
            break;
        }

        // FILENAME
        if(!write_string(filename.c_str())) {
            break;
        }

        // MODE
        if(!write_string(mode.c_str())) {
            break;
        }

#ifdef DEBUG
        std::cout << "PACKET filled!" << std::endl;
#endif

        log += "file: " + filename;

        // OPTIONS
        return write_options();
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

        this->log += "block number " + std::to_string(block_num);
        this->block_num++;
        return true;
    } while(0);

    std::cerr << "Error while creating ACK packet" << std::endl;
    return false;
}

bool Tftp_client::fill_DATA()
{
    int c;
    bool ok = false;

    this->out_curr_pos = 0; // reinitialize
    this->exp_type = OPCODE_ACK;
    this->active_cr = false;

    do {
        if(!write_word(OPCODE_DATA)) {
           break;
        }

        if(!write_word(this->block_num)) {
            break;
        }

        ok = true;
    } while(0);

    if(!ok) {
        std::cerr << "Error while creating DATA packet!" << std::endl;
        return false;
    }

    // write bytes which were processed but didn't fit last block
    for(size_t i = 0; i < this->bytes_left.size(); i++) {
        if(!write_byte(this->bytes_left[i])) {
            return false;
        }
    }
    this->bytes_left.clear();

    // try to fill another block of data
    while(this->out_curr_pos - 4 < this->block_size) {
        c = this->file.get();

        // end of file reached => last block
        if(c == EOF) {
            this->last = true;
            break;
        }

        if(!write_byte(c)) {
            std::cerr << "Error while writing data into DATA packet!" << std::endl;
            return false;
        }
    }

    this->cur_size += this->out_curr_pos - 4;
    return true;
}

bool Tftp_client::fill_ERROR(err_code_t code, std::string msg)
{
    this->out_curr_pos = 0; // reinitialize

    do {
        if(!write_word(OPCODE_ERROR)) {
            break;
        }

        if(!write_word(code)) {
            break;
        }

        if(!write_string(msg.c_str())) {
            break;
        }

        this->log += "code: " + std::to_string(code) + ", msg: " + msg;
        this->exp_resp = false;
       return true;
    } while(0);

    std::cerr << "Error while creating ERROR packet!" << std::endl;
    return false;
}

// PRIVATE INSTANCE METHODS FOR ACCESSING DATA IN RECIEVED PACKETS

bool Tftp_client::read_type(uint16_t &res)
{
    this->in_curr_pos = 0;

    return read_word(res);
}

bool Tftp_client::read_cr(uint8_t &res)
{
    switch(this->in_buffer[this->in_curr_pos]) {
    case '\n':
        res = '\n';
        break;
    case '\0':
        res = '\r';
        break;
    default:
        return false;
    }

    this->active_cr = false;
    this->in_curr_pos++;
    return true;
}

bool Tftp_client::read_byte(uint8_t &res)
{
    if(this->in_curr_pos >= this->resp_len) {
        return false;
    }

    if(this->active_cr) {
        return read_cr(res);
    }

    // netascii mode - current byte is CR
    if(!this->binary && this->in_buffer[this->in_curr_pos] == '\r') {
        this->active_cr = true;
        this->in_curr_pos++;
        return true;
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

// PRIVATE INSTANCE METHODS FOR PARSING RECIEVED TFTP PACKETS

bool Tftp_client::parse_ERROR()
{
    uint16_t err_code;
    std::string err_msg;
    this->last = true;

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

        if(this->exp_type == OPCODE_OACK && err_code == ERR_CODE_PROBLEMATIC_OPTION) {
            this->options.clear();
            this->last = false;
            this->first = true;
            this->resend_rq = true;
            reset_TID();
        }
        return true;
    } while(0);

    std::cerr << "Error while parsing ERROR packet!" << std::endl;
    return false;
}

bool Tftp_client::parse_ACK()
{
    uint16_t block_num;

    if(!read_word(block_num)) {
        std::cerr << "Error while parsing ACK packet!" << std::endl;
        return false;
    }

    if(this->in_curr_pos != this->resp_len) {
        std::cerr << "Invalid ACK packet format!" << std::endl;
        return false;
    }

    this->log += "block number " + std::to_string(block_num);

    // duplicate ACK packets are ignored
    if(block_num < this->block_num) {
        this->send_type = OPCODE_SKIP;
        this->log += " (duplicate - will be ignored)";
        return true;
    }

    this->send_type = OPCODE_DATA;
    this->block_num++;
    return true;
}

bool Tftp_client::parse_DATA()
{
    uint64_t data_size = this->resp_len - 4;
    uint16_t block_num;
    uint8_t c;
    this->cur_size += data_size;
    this->active_cr = false;
    this->exp_resp = data_size == this->block_size;
    this->send_type = OPCODE_ACK;

    if(!read_word(block_num)) {
        std::cerr << "Error while reading block number from DATA packet!" << std::endl;
        return false;
    }

#ifdef DEBUG
    std::cout << "TFTP DATA - block: " << block_num << std::endl;
#endif

    // block number cannot be 0
    if(block_num == 0) {
        return false;
    }

    this->log += "block number " + std::to_string(block_num) + ", ";

    // duplicate DATA packet means resend of last ACK packet
    if(block_num < this->block_num) {
        this->log += " (duplicate - last ACK packet has been resent)";
        this->send_type = OPCODE_SKIP;
        this->resend_timer = std::time(0) + TIMEOUT;
        return send_packet();
    }

    // CR byte from previous data block
    if(!this->bytes_left.empty()) {

        this->active_cr = true;
    }
    this->bytes_left.clear();

#ifdef DEBUG
    std::cout << "----------------------------------\n";
#endif
    // try to read and store recieved data block
    while(this->in_curr_pos < this->resp_len) {
        if(!read_byte(c)) {
            std::cerr << "Error while reading DATA packet!" << std::endl;
            return false;
        }

        if(!this->active_cr) {
            this->file.put(c);
#ifdef DEBUG
            std::cout << c;
#endif
        }
    }
#ifdef DEBUG
    std::cout << "----------------------------------\n";
#endif

    // second byte of CR sequence didn't fit in this block
    if(this->active_cr) {
        // invalid message in netascii - CR sequence
        if(!this->exp_resp) {
            std::cerr << "Error! CR sequence in last block wasn't end properly!" << std::endl;
            return false;
        }

        this->bytes_left.push_back('\r');
    }

    // create log information
    this->log += std::to_string(data_size) + " bytes ";

    return true;
}

bool Tftp_client::parse_OACK()
{
    std::string option;
    std::string value;

    // duplicate OACK packet is ignored
    if(this->exp_type != OPCODE_OACK) {
        this->send_type = OPCODE_SKIP;
        return true;
    }

    // for read request OACK is followed by ACK num 0
    if(this->send_type == OPCODE_RRQ) {
        this->send_type = OPCODE_ACK;
        this->block_num = 0;
    // for write request OACK is followed by DATA num 1
    } else {
        this->send_type = OPCODE_DATA;
        this->block_num = 1;
    }

    while(this->in_curr_pos < this->resp_len) {
        if(!read_string(option)) {
            return false;
        }

        if(!read_string(value)) {
            return false;
        }

        if(!validate_option(option, value)) {
            return false;
        }

#ifdef DEBUG
        std::cout << "Option: " << option << " value: " << value << std::endl;
#endif
    }

    for(auto it = this->options.begin(); it != this->options.end(); it++) {
        log += (it == this->options.begin())? "" : ", ";
        log += it->first;
        log += (it->second.empty())? " (confirmed)" : " (not confirmed)"; 
    }

    return realloc_buffers();
}

bool Tftp_client::validate_option(std::string option, std::string value)
{
    bool ret = true;

    if(this->options.find(option) == this->options.end()) {
        return false;
    }

    if(option == "tsize") {
        this->tsize = std::stoul(value);
        ret = this->binary; // valid only for binary mode
    } else if(option == "timeout") {
        ret = this->options[option] == value; // timeout value must match
    } else if(option == "blksize") {
        this->block_size = std::stoul(value);
        ret = this->block_size <= std::stoul(this->options[option]); // must by less than or equel than proposed
    }

    this->options[option].clear();
    return ret;
}

bool Tftp_client::realloc_buffers()
{
    const uint64_t new_size = this->block_size + TFTP_HEADER;
    uint8_t *buf;

    if(this->size < new_size) {
        // resieze output buffer
        if((buf = resize(this->out_buffer.release(), new_size)) == nullptr) {
            return false;
        }

        this->out_buffer.reset(buf);

        // resieze input buffer
        if((buf = resize(this->in_buffer.release(), new_size)) == nullptr) {
            return false;
        }

        this->in_buffer.reset(buf);
        this->size = new_size;

        memset(static_cast<void *> (this->out_buffer.get()), 0, new_size);
        memset(static_cast<void *> (this->in_buffer.get()), 0, new_size);
    }

    return true;
}