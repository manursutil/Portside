#include "commands.h"
#include "clients.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

typedef enum {
  CMD_HELP,
  CMD_QUIT,
  CMD_NAME,
  CMD_USERS,
  CMD_MSG,
  CMD_UNKNOWN
} Command;

// TODO: Sanitize command buffer input

static void sanitize_command(char *buffer) {
  size_t len = strlen(buffer);

  while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
    buffer[--len] = '\0';
  }
}

static Command command_parser(const char *cmd) {
  if (strcmp(cmd, "help") == 0)
    return CMD_HELP;
  if (strcmp(cmd, "quit") == 0)
    return CMD_QUIT;
  if (strcmp(cmd, "name") == 0)
    return CMD_NAME;
  if (strcmp(cmd, "users") == 0)
    return CMD_USERS;
  if (strcmp(cmd, "msg") == 0)
    return CMD_MSG;

  return CMD_UNKNOWN;
}

static void handle_help(int fd) {
  char *help_msg = "\nAvailable commands:\n- /help: prints help message\n- "
                   "/quit: leave the server\n- /name <username>: change your "
                   "username\n- /msg <user> <message>: message a user "
                   "privately\n- /users: lists all the users in the server\n\n";
  send(fd, help_msg, strlen(help_msg), 0);
}

static void list_users(int fd, Clients clients) {
  for (size_t i = 0; i < clients.size; i++) {
    Client *c = &clients.items[i];

    char msg_buf[64];
    snprintf(msg_buf, sizeof(msg_buf), "%d: %s\n", (int)i, c->username);
    send(fd, msg_buf, strlen(msg_buf), 0);
  }
}

static int handle_name_change(char *new_username, int fd, Clients *clients) {
  Client *c = clients_search_fd(clients, fd);
  if (c == NULL) {
    return -1;
  }

  strncpy(c->username, new_username, sizeof(c->username));
  c->username[sizeof(c->username) - 1] =
      '\0'; // truncate usernames that are too long

  return 0;
}

static void handle_private_msg(int fd, char *username, Clients clients,
                               const char *message) {
  // get sender
  Client *sender = clients_search_fd(&clients, fd);

  // get receiver
  Client *receiver = clients_search_username(&clients, username);
  if (receiver == NULL) {
    char *msg = "Could not find that user in the server.\n";
    send(fd, msg, strlen(msg), 0);
    return;
  }

  // Send message from sender to receiver
  char recv_buf[1024];
  snprintf(recv_buf, sizeof(recv_buf), "***%s sent you a private message!\n",
           sender->username);
  send(receiver->fd, recv_buf, strlen(recv_buf), 0);
  send(receiver->fd, message, strlen(message), 0);

  // Confirmation message to sender
  char msg_buf[1024];
  snprintf(msg_buf, sizeof(msg_buf), "Private message sent to: %s\n",
           receiver->username);
  send(sender->fd, msg_buf, strlen(msg_buf), 0);
  return;
}

bool handle_command(char *cmd, int fd, Clients *clients) {
  sanitize_command(cmd);

  char *args = strchr(cmd, ' ');
  if (args != NULL) {
    *args++ = '\0';
    while (*args == ' ')
      args++;
  }

  switch (command_parser(cmd)) {
  case CMD_HELP:
    handle_help(fd);
    break;
  case CMD_QUIT:
    return true;
  case CMD_NAME:
    if (args == NULL || *args == '\0') {
      char *msg = "Usage: /name <username>\n";
      send(fd, msg, strlen(msg), 0);
      break;
    }
    handle_name_change(args, fd, clients);
    break;
  case CMD_MSG: {
    char *username = args;
    char *message = args ? strchr(args, ' ') : NULL;

    if (message != NULL) {
      *message++ = '\0';
      while (*message == ' ') {
        message++;
      }
    }

    if (username == NULL || *username == '\0' || message == NULL ||
        *message == '\0') {
      char *usage = "Usage /msg <username> <message>\n";
      send(fd, usage, strlen(usage), 0);
      break;
    }

    handle_private_msg(fd, username, *clients, message);
    break;
  }
  case CMD_USERS:
    list_users(fd, *clients);
    break;
  default: {
    const char *error_msg =
        "Unknown command. (use '/help', '/users', '/name', '/quit, '/msg')\n";
    send(fd, error_msg, strlen(error_msg), 0);
    break;
  }
  }

  return false;
}
