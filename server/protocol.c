#include "protocol.h"

#include <stdio.h>

int prepare_broadcast_message(char *out, size_t out_size, const Client *client,
                              const char *message) {
  int len = snprintf(out, out_size, "%s: %s\n", client->username, message);

  if (len < 0)
    return -1;

  if ((size_t)len >= out_size)
    return -1; // message doesn't fit

  return len;
}
