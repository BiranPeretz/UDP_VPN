#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>

#include "tun.h"
#include "io.h"

static void die(const char *msg) { perror(msg); exit(1); }

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s <server_host> <port> [ifname]\n", argv[0]);
        return 2;
    }
    const char *host   = argv[1];
    uint16_t    port   = (uint16_t)atoi(argv[2]);
    const char *ifhint = (argc == 4) ? argv[3] : "";

    struct tun_device td;
    if (tun_open(&td, ifhint) < 0) die("tun_open");
    if (tun_set_nonblock(&td, 1) < 0) die("tun_set_nonblock");

    int sock = udp_connect(host, port, 0);
    if (sock < 0) die("udp_connect");
    if (io_set_nonblock(sock, 1) < 0) die("io_set_nonblock");

    fprintf(stderr, "[client] TUN=%s fd=%d, UDP connected to %s:%u fd=%d\n",
            td.name, td.fd, host, (unsigned)port, sock);

    unsigned char buf[65536];

    for (;;) {
        struct pollfd p[2] = {
            { .fd = td.fd, .events = POLLIN, .revents = 0 },
            { .fd = sock,  .events = POLLIN, .revents = 0 },
        };
        int rc = poll(p, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            die("poll");
        }

        /* TUN -> UDP */
        if (p[0].revents & POLLIN) {
            ssize_t n = tun_read(&td, buf, sizeof(buf));
            if (n > 0) {
                ssize_t w = send(sock, buf, (size_t)n, 0);
                (void)w;
            }
        }

        /* UDP -> TUN */
        if (p[1].revents & POLLIN) {
            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n > 0) {
                (void)tun_write(&td, buf, (size_t)n);
            }
        }
    }
}
