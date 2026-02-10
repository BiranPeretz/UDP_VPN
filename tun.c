#include <errno.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tun.h"


static int set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}


int tun_open(struct tun_device *td, const char *ifname_hint) {
    if (!td) { errno = EINVAL; return -1; }
    memset(td, 0, sizeof(*td));
    td->fd = -1;


    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    set_cloexec(fd);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (ifname_hint && ifname_hint[0] != '\0') {

        strncpy(ifr.ifr_name, ifname_hint, IF_NAMESIZE - 1);
        ifr.ifr_name[IF_NAMESIZE - 1] = '\0';
    }

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    td->fd = fd;
    strncpy(td->name, ifr.ifr_name, IF_NAMESIZE);
    td->name[IF_NAMESIZE - 1] = '\0';
    return 0;
}

void tun_close(struct tun_device *td) {
    if (!td) return;
    if (td->fd >= 0) {
        close(td->fd);
        td->fd = -1;
    }
    td->name[0] = '\0';
}

int tun_set_nonblock(const struct tun_device *td, int on) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    int flags = fcntl(td->fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (on) flags |= O_NONBLOCK;
    else    flags &= ~O_NONBLOCK;
    return fcntl(td->fd, F_SETFL, flags);
}

int tun_set_persist(const struct tun_device *td, int on) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    return ioctl(td->fd, TUNSETPERSIST, on ? 1 : 0);
}

int tun_set_owner(const struct tun_device *td, uid_t uid) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    return ioctl(td->fd, TUNSETOWNER, uid);
}

int tun_set_group(const struct tun_device *td, gid_t gid) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    return ioctl(td->fd, TUNSETGROUP, gid);
}

ssize_t tun_read(const struct tun_device *td, void *buf, size_t len) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    for (;;) {
        ssize_t n = read(td->fd, buf, len);
        if (n < 0 && errno == EINTR) continue;
        return n;
    }
}

ssize_t tun_write(const struct tun_device *td, const void *buf, size_t len) {
    if (!td || td->fd < 0) { errno = EBADF; return -1; }

    size_t off = 0;
    while (off < len) {
        ssize_t n = write(td->fd, (const char*)buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return (ssize_t)len;
}
