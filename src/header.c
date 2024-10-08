#include "header.h"
#include "utils.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int tcp_read_options(tcp_header *header, uint8_t *buffer) {
    int opt_len = (header->doff << 2) - TCP_HEADER_SIZE;

    int nbytes = 0;
    int written = 0;
    while (nbytes < opt_len) {
        uint8_t opt_kind = buffer[nbytes];
        nbytes++;
        switch (opt_kind) {
        case TCP_MSS:
            memcpy(header->opts + written, buffer + nbytes - 1, 4);
            written += 4;

            nbytes += buffer[nbytes];
            break;
        case TCP_EOL:
        case TCP_NOOP:
            break;
        default:
            printf("Option not supported");
            nbytes += buffer[nbytes] - 1;
            break;
        }
    }

    return opt_len;
}

int tcp_write_options(tcp_header *header, uint8_t *buffer) {
    int opt_len = (header->doff << 2) - TCP_HEADER_SIZE;

    memcpy(header->opts, buffer, opt_len);

    return 0;
}

tcp_header create_tcp_header(uint16_t src_port, uint16_t dest_port) {
    tcp_header tcph;
    tcph.src_port = src_port;
    tcph.dest_port = dest_port;

    tcph.seq = 0;

    tcph.seq_ack = 0;

    tcph.doff = 5;
    tcph.res = 0;
    tcph.flags = 0;
    tcph.wnd = 1024;

    tcph.check = 0;
    tcph.urg_ptr = 0;

    return tcph;
}

int to_tcp_header(tcp_header *header, uint8_t *buffer) {
    memcpy(header, buffer, TCP_HEADER_SIZE);

    convert_tcp_header_he(header);

    tcp_read_options(header, buffer + TCP_HEADER_SIZE);
    return header->doff << 2;
}

int from_tcp_header(tcp_header *header, tcp_ip_header *ip_header,
                    uint8_t *payload, uint8_t *buffer) {
    tcp_header tcph_ne;
    tcp_ip_header piph_ne;
    memcpy(&tcph_ne, header, header->doff << 2);
    memcpy(&piph_ne, ip_header, TCP_IP_HEADER_SIZE);

    convert_tcp_header_ne(&tcph_ne);
    convert_tcp_ip_header_ne(&piph_ne);

    tcp_checksum(&piph_ne, &tcph_ne, payload);

    memcpy(buffer, &tcph_ne, header->doff << 2);
    return header->doff << 2;
}

int convert_tcp_header_he(tcp_header *header) {
    header->src_port = ntohs(header->src_port);
    header->dest_port = ntohs(header->dest_port);

    header->seq = ntohl(header->seq);
    header->seq_ack = ntohl(header->seq_ack);

    header->wnd = ntohs(header->wnd);
    header->urg_ptr = ntohs(header->urg_ptr);
    return 0;
}

int convert_tcp_header_ne(tcp_header *header) {

    header->src_port = htons(header->src_port);
    header->dest_port = htons(header->dest_port);

    header->seq = htonl(header->seq);
    header->seq_ack = htonl(header->seq_ack);

    header->wnd = htons(header->wnd);
    header->urg_ptr = htons(header->urg_ptr);

    return 0;
}

ip_header create_ip_header(uint32_t src_addr, uint32_t dest_addr,
                           uint16_t data_len) {
    ip_header iph;

    iph.ver = 4;
    iph.ihl = 5;
    iph.tos = 0;
    iph.len = data_len;
    iph.id = 0;
    iph.frag = 0;
    iph.ttl = 10;
    iph.proto = (uint8_t)IP_PROTO_TCP;
    iph.check = 0;
    iph.src_addr = src_addr;
    iph.dest_addr = dest_addr;

    return iph;
}

int to_ip_header(ip_header *header, uint8_t *buffer) {
    memcpy(header, buffer, IP_HEADER_SIZE);
    convert_ip_header_he(header);

    return IP_HEADER_SIZE;
}

int from_ip_header(ip_header *header, uint8_t *buffer) {
    ip_header iph_ne;
    memcpy(&iph_ne, header, IP_HEADER_SIZE);
    convert_ip_header_ne(&iph_ne);
    ip_checksum(&iph_ne);
    memcpy(buffer, &iph_ne, IP_HEADER_SIZE);
    return IP_HEADER_SIZE;
}

int convert_ip_header_ne(ip_header *header) {

    header->src_addr = htonl(header->src_addr);
    header->dest_addr = htonl(header->dest_addr);

    header->len = htons(header->len);
    header->frag = htons(header->frag);
    header->id = htons(header->id);

    return 0;
}

int convert_ip_header_he(ip_header *header) {
    header->src_addr = ntohl(header->src_addr);
    header->dest_addr = ntohl(header->dest_addr);

    header->len = ntohs(header->len);
    header->frag = ntohs(header->frag);
    header->id = ntohs(header->id);

    return 0;
}

int convert_tcp_ip_header_ne(tcp_ip_header *header) {
    header->src_addr = htonl(header->src_addr);
    header->dest_addr = htonl(header->dest_addr);
    header->protocol = htons(header->protocol);
    header->tcp_len = htons(header->tcp_len);
    return 0;
}

uint16_t checksum(uint16_t *payload, uint32_t count, uint32_t start) {
    uint32_t sum = start;
    while (count > 1) {
        sum += *payload++;
        count -= 2;
    }

    if (count > 0) {
        sum += (*payload) & htons(0xFF00);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

uint16_t tcp_checksum(tcp_ip_header *piph, tcp_header *tcph, uint8_t *payload) {
    tcph->check = 0;

    uint8_t tcp_len = ntohs(piph->tcp_len);

    uint32_t sum = 0;

    sum += (piph->src_addr >> 16) & 0xFFFF;
    sum += (piph->src_addr) & 0xFFFF;

    sum += (piph->dest_addr >> 16) & 0xFFFF;
    sum += (piph->dest_addr) & 0xFFFF;

    sum += piph->protocol;

    sum += piph->tcp_len;

    tcph->check = 0;

    for (int i = 0; i < (tcph->doff) << 1; i++) {
        sum += *((uint16_t *)tcph + i);
    }

    tcph->check =
        checksum((uint16_t *)payload, tcp_len - (tcph->doff << 2), sum);

    return tcph->check;
}

uint16_t ip_checksum(ip_header *iph) {
    iph->check = 0;
    iph->check = checksum((uint16_t *)iph, iph->ihl << 2, 0);
    return iph->check;
}

int wrapping_lt(uint32_t left, uint32_t right) {
    return left - right > (1 << 31);
}

int wrapping_between(uint32_t left, uint32_t middle, uint32_t right) {
    return wrapping_lt(left, middle) && wrapping_lt(middle, right);
}

uint32_t wrapping_len(uint32_t left, uint32_t right) {
    return right >= left ? right - left
                         : (uint32_t)(1 << 31) - (left - (right + 1));
}
