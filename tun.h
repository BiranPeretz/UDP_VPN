#ifndef VPN_TUN_H
#define VPN_TUN_H

#include <stddef.h>
#include <sys/types.h>
#include <net/if.h>

struct tun_device {
    int  fd;
    char name[IF_NAMESIZE];
};

int tun_open(struct tun_device *td, const char *ifname_hint);

void tun_close(struct tun_device *td);

int tun_set_nonblock(const struct tun_device *td, int on);

int tun_set_persist(const struct tun_device *td, int on);

int tun_set_owner(const struct tun_device *td, uid_t uid);

int tun_set_group(const struct tun_device *td, gid_t gid);

ssize_t tun_read(const struct tun_device *td, void *buf, size_t len);
ssize_t tun_write(const struct tun_device *td, const void *buf, size_t len);


#endif /*VPN_TUN_H*/
