#ifndef VPN_IO_H
#define VPN_IO_H

#include <stddef.h> /*size_t*/
#include <sys/types.h> /*ssize_t*/
#include <stdint.h> /*uint16_t*/

int     io_set_nonblock(int file_descriptor, int on);
ssize_t io_read_full (int file_descriptor, void *data_buffer, size_t bytes_to_read);
ssize_t io_write_full(int file_descriptor, const void *data_buffer, size_t bytes_to_write);



/*
1) resolve host:port
2) create non-blocking datagram socket
3) connect()
*/
int udp_connect(const char *host, uint16_t port, int timeout_ms); /*return file_descriptor on success, otherwise -1 on error*/


/*bind non-blocking datagram socket to bind_host:port*/
int udp_bind(const char *bind_host, uint16_t port); /*return file_descriptor on success, otherwise -1 on error*/

#endif /*VPN_IO_H*/
