#include "header.h"
#include "utils.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int tun_alloc(char *dev, int flags) {
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";
    if ((fd = open(clonedev, O_RDWR)) < 0) {
        perror("Opening /dev/net/tun");
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr))) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}

int main(int argc, char **argv) {
    int sender = 0;
    if (argc > 0) {
        sender = atoi(argv[1]);
    }

    int tun_fd;
    char tun_name[IFNAMSIZ];
    uint8_t buffer[4096];
    int nwrite;
    unsigned long x = 0;
    strcpy(tun_name, "tun0");
    tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        perror("Allocating interface");
        exit(1);
    }

    if (sender > 0) {
        while (1) {
            char data[2] = {'0', '1'};
            nwrite = write(tun_fd, data, sizeof(data));
            if (nwrite < 0) {
                perror("writing data");
            }
            printf("Write %d bytes\n", nwrite);
            sleep(1);
        }
    } else {
        while (1) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread < 0) {
                perror("Reading from interface");
                close(tun_fd);
                exit(1);
            }

            ip_header iph;

            to_ip_header(&iph, buffer);

            if (iph.ver != 4)
                continue;

            printf("\n");

            printf("nread: %d\n", nread);

            print_hex(buffer, nread);

            print_ip_header(&iph);
            ip_checksum(&iph);

            printf("ipchecksum: %04X\n", iph.check);

            tcp_header tcph;

            to_tcp_header(&tcph, buffer + (iph.ihl << 2));

            print_tcp_header(&tcph);

            uint8_t buf[4096];

            uint32_t temp = iph.src_addr;
            iph.src_addr = iph.dest_addr;
            iph.dest_addr = temp;
            iph.len = htons(IP_HEADER_SIZE + TCP_HEADER_SIZE);

            uint8_t empty[0];

            tcp_ip_header piph;
            piph.zero = 0;
            piph.dest_addr = iph.dest_addr;
            piph.src_addr = iph.src_addr;
            piph.protocol = IP_PROTO_TCP;
            piph.tcp_len = 32;

            uint16_t temp1 = tcph.src_port;
            tcph.src_port = tcph.dest_port;
            tcph.dest_port = temp1;
            tcph.seq_ack = htonl(ntohl(tcph.seq) + 1);
            tcph.seq = 0;
            tcph.flags |= TCP_FLAG_ACK;
            tcph.doff = 5;
            tcph.check = tcp_checksum(&piph, &tcph, empty);

            printf("=================================\n");

            print_ip_header(&iph);
            print_tcp_header(&tcph);

            from_ip_header(&iph, buf);
            from_tcp_header(&tcph, buf + IP_HEADER_SIZE);

            printf("len: %d\n", ntohs(iph.len));

            print_hex(buf, ntohs(iph.len));

            write(tun_fd, buf, ntohs(iph.len));

            printf("\n");
        }
    }

    return 0;
}
