#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tun.h"
#include "io.h"

static void die(const char *msg) { perror(msg); exit(1); }

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s <port> [bind_host] [ifname]\n", argv[0]);
        fprintf(stderr, "examples:\n");
        fprintf(stderr, "%s 5555 (bind on all addrs)\n", argv[0]);
        fprintf(stderr, "%s 5555 0.0.0.0 tun0\n", argv[0]);
        return 2;
    }
    uint16_t    port    = (uint16_t)atoi(argv[1]);
    const char *bindip  = (argc >= 3) ? argv[2] : "";
    const char *ifhint  = (argc == 4) ? argv[3] : "";

    struct tun_device td;
    if (tun_open(&td, ifhint) < 0) die("tun_open");
    if (tun_set_nonblock(&td, 1) < 0) die("tun_set_nonblock");

    int sock = udp_bind(bindip, port);
    if (sock < 0) die("udp_bind");
    if (io_set_nonblock(sock, 1) < 0) die("io_set_nonblock");

    fprintf(stderr, "[server] TUN=%s fd=%d, UDP bound on %s:%u fd=%d\n",
            td.name, td.fd, (bindip && bindip[0]) ? bindip : "*", (unsigned)port, sock);

    unsigned char buf[65536];

    struct sockaddr_storage peer;
    socklen_t peer_len = 0;
    int have_peer = 0;

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

        /* UDP -> TUN */
        if (p[1].revents & POLLIN) {
            struct sockaddr_storage src;
            socklen_t srclen = sizeof(src);
            ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                                 (struct sockaddr *)&src, &srclen);
            if (n > 0) {
                if (!have_peer) { peer = src; peer_len = srclen; have_peer = 1; }
                (void)tun_write(&td, buf, (size_t)n);
            }
        }

        /* TUN -> UDP */
        if (have_peer && (p[0].revents & POLLIN)) {
            ssize_t n = tun_read(&td, buf, sizeof(buf));
            if (n > 0) {
                (void)sendto(sock, buf, (size_t)n, 0,
                             (struct sockaddr *)&peer, peer_len);
            }
        }
    }
}
