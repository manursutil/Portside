#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "clients.h"
#include <sys/types.h>

void trim_newline(char *buffer, ssize_t *bytes);
int prepare_broadcast_message(char *out, size_t out_size, const Client *client,
                              const char *message);
int send_all(int fd, const char *buffer, size_t len);

#endif // !PROTOCOL_H
