#include "commands.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void expect_reply(int fd, const char *expected) {
  char buffer[256] = {0};
  size_t used = 0;
  ssize_t n;

  while ((n = recv(fd, buffer + used, sizeof(buffer) - 1 - used, 0)) > 0) {
    used += (size_t)n;
    assert(used < sizeof(buffer) - 1);
  }

  assert(n == 0);
  assert(strcmp(buffer, expected) == 0);
}

int main(int argc, char **argv) {
  int sender[2], receiver[2];

  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sender) == 0);
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, receiver) == 0);

  Clients clients = {.size = 2};
  clients.items[0] =
      (Client){.fd = sender[0], .username = "alice", .state = CLIENT_CONNECTED};
  clients.items[1] =
      (Client){.fd = receiver[0], .username = "bob", .state = CLIENT_CONNECTED};

  char command[64];

  assert(argc == 2);
  assert(strlen(argv[1]) < sizeof(command));

  strcpy(command, argv[1]);

  assert(!handle_command(command, sender[0], &clients));
  assert(shutdown(sender[0], SHUT_WR) == 0);
  assert(shutdown(receiver[0], SHUT_WR) == 0);

  if (strcmp(argv[1], "msg bob hello") == 0 ||
      strcmp(argv[1], "msg   bob   hello  \r\n") == 0) {
    expect_reply(sender[1], "Private message sent to: bob\n");
    expect_reply(receiver[1], "***alice sent you a private message!\nhello\n");

  } else if (strcmp(argv[1], "msg nobody hello") == 0) {
    expect_reply(sender[1], "Could not find that user in the server.\n");
    expect_reply(receiver[1], "");

  } else {
    expect_reply(sender[1], "Usage /msg <username> <message>\n");
    expect_reply(receiver[1], "");
  }

  close(sender[0]);
  close(sender[1]);
  close(receiver[0]);
  close(receiver[1]);

  puts("PASS");
}
