#include "protocol.h"

#include <stdio.h>

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
