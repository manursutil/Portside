#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <sys/types.h>

void trim_newline(char *buffer, ssize_t *bytes);
int send_all(int fd, const char *buffer, size_t len);

#endif // !NET_H
