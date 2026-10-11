#include "protocol.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

// telnet adds \r\n to each line sent
// trim these newline characters for strcmp to work as expected
void trim_newline(char *buffer, ssize_t *bytes) {
  while (*bytes > 0 &&
         (buffer[*bytes - 1] == '\n' || buffer[*bytes - 1] == '\r')) {
    (*bytes)--;
  }

  buffer[*bytes] = '\0';
}

int prepare_broadcast_message(char *out, size_t out_size, const Client *client,
                              const char *message) {
  int len = snprintf(out, out_size, "%s: %s\n", client->username, message);

  if (len < 0)
    return -1;

  if ((size_t)len >= out_size)
    return -1; // message doesn't fit

  return len;
}

int send_all(int fd, const char *buffer, size_t len) {
  size_t total = 0;
  size_t bytes_left = len;
  ssize_t n = 0;

  while (total < len) {
    n = send(fd, buffer + total, bytes_left, 0);
    if (n == -1) {
      perror("send_all");
      break;
    }

    total += n;
    bytes_left -= n;
  }

  return n == -1 ? -1 : 0;
}
