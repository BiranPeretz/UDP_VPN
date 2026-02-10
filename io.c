#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>


#include "io.h"


int io_set_nonblock(int file_descriptor, int on) {
    int flags = fcntl(file_descriptor, F_GETFL, 0);

    if(flags < 0) {
        return -1;
    }

    if(on) {
        flags |= O_NONBLOCK;

    } else {
        flags &= ~O_NONBLOCK;
    
    }

    return fcntl(file_descriptor, F_SETFL, flags);
}


/*read bytes_to_read bytes to given file_descriptor*/
ssize_t io_read_full(int file_descriptor, void *data_buffer, size_t bytes_to_read) {
    size_t bytes_red = 0;
    ssize_t result = 0;

    while(bytes_red < bytes_to_read) {
        result = read(file_descriptor, (char*)data_buffer + bytes_red, bytes_to_read - bytes_red);

        if(result > 0) {
            bytes_red += (size_t)result;
            continue;
        }

        if(result == 0) { /*if EOF*/
            return (ssize_t)bytes_red;
        }

        if(errno == EINTR) {
            continue;
        }
    
        return -1; /*if reached, unknown error*/
    }

    return (ssize_t)bytes_red;
}


/*write bytes_to_write bytes to given file_descriptor*/
ssize_t io_write_full(int file_descriptor, const void *data_buffer, size_t bytes_to_write) {
    size_t bytes_written = 0;
    ssize_t result = 0;

    while(bytes_written < bytes_to_write) {
        result = write(file_descriptor, (const char*)data_buffer + bytes_written, bytes_to_write - bytes_written);

        if(result > 0) {
            bytes_written += (size_t)result;
            continue;
        }

        if(result < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -1; /*if reached, unknown error*/
        }
    }

    return (ssize_t)bytes_written;
}

/*set file_descriptor's close-on-exec*/
static int set_cloexec(int file_descriptor) {
    int flags;

    flags = fcntl(file_descriptor, F_GETFD);
    if(flags < 0) {
        return -1;
    }

    return fcntl(file_descriptor, F_SETFD, flags | FD_CLOEXEC);
}

/*convert port number to string*/
static void port_to_string(uint16_t port, char out[6]) {
    snprintf(out, 6, "%u", (unsigned)port);
}

/*connect UDP socket to host:port*/
int udp_connect(const char *host, uint16_t port, int timeout_ms) {
    char port_string[6];
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *ai;
    int rc;
    int file_descriptor;

    (void)timeout_ms;
    if(!host) {
        errno = EINVAL;
        return -1;
    }

    port_to_string(port, port_string);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   /*either IPv4/IPv6*/
    hints.ai_socktype = SOCK_DGRAM;  /*UDP*/
    hints.ai_protocol = IPPROTO_UDP;

    result = NULL;
    rc = getaddrinfo(host, port_string, &hints, &result);
    if(rc != 0) {
        errno = EINVAL;
        return -1;
    }

    file_descriptor = -1;
    for(ai = result; ai != NULL; ai = ai->ai_next) {
        file_descriptor = socket(ai->ai_family,
                                 ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                                 ai->ai_protocol);
        if(file_descriptor < 0) {
            continue;
        }

        /*connect() sets default peer; returns immediately for UDP*/
        if(connect(file_descriptor, ai->ai_addr, ai->ai_addrlen) == 0) {
            freeaddrinfo(result);
            (void)set_cloexec(file_descriptor);
            return file_descriptor;
        }

        close(file_descriptor);
        file_descriptor = -1;
    }

    freeaddrinfo(result);
    return -1;
}
/*bind UDP socket on bind_host:port*/
int udp_bind(const char *local_host, uint16_t local_port) {
    char port_string[6];
    struct addrinfo hints;
    struct addrinfo *resolved_addresses;
    struct addrinfo *current_address;
    int getaddrinfo_result;
    int socket_descriptor;
    int reuse_option;

    port_to_string(local_port, port_string);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags    = AI_PASSIVE;

    resolved_addresses = NULL;
    getaddrinfo_result = getaddrinfo((local_host && local_host[0]) ? local_host : NULL,
                                     port_string, &hints, &resolved_addresses);
    if(getaddrinfo_result != 0) {
        errno = EINVAL;
        return -1;
    }

    socket_descriptor = -1;
    for(current_address = resolved_addresses; current_address != NULL; current_address = current_address->ai_next) {
        socket_descriptor = socket(current_address->ai_family,
                                   current_address->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                                   current_address->ai_protocol);
        if(socket_descriptor < 0) {
            continue;
        }

        reuse_option = 1;
        (void)setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &reuse_option, sizeof(reuse_option));
#ifdef SO_REUSEPORT
        (void)setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &reuse_option, sizeof(reuse_option));
#endif

        if(bind(socket_descriptor, current_address->ai_addr, current_address->ai_addrlen) == 0) {
            freeaddrinfo(resolved_addresses);
            (void)set_cloexec(socket_descriptor);
            return socket_descriptor;
        }

        close(socket_descriptor);
        socket_descriptor = -1;
    }

    freeaddrinfo(resolved_addresses);
    return -1;
}
