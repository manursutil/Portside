#include "net.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

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
