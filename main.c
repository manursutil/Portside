#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "clients.h"
#include "commands.h"
#include "protocol.h"

#define PORT 8080
#define MAX_BUFFER_SIZE 1024
#define PROMPT "> "

void broadcast(const Clients *clients, int sender_fd, const char *message) {
  size_t len = strlen(message);
  for (size_t i = 0; i < clients->size; i++) {
    const Client *c = &clients->items[i];
    if (c->fd != sender_fd && c->state == CLIENT_CONNECTED) {
      send(c->fd, message, len, 0);
      send(c->fd, PROMPT, strlen(PROMPT), 0);
    }
  }
}

int create_server_socket(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  int yes = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    perror("setsockopt");
    close(fd);
    return -1;
  }

  // initialize the socket address struct
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;         // IPv4
  address.sin_addr.s_addr = INADDR_ANY; // any local interface
  address.sin_port = htons(PORT);       // port

  // bind socket to a port
  if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    close(fd);
    return -1;
  }

  // listen to connections
  if (listen(fd, 10) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }

  return fd;
}

void disconnect_client(Clients *clients, int fd, fd_set *master_set) {
  Client *c = clients_search_fd(clients, fd);

  if (c != NULL && c->state == CLIENT_CONNECTED) {
    printf("Client %s disconnected.\n", c->username);

    char left[MAX_NAME + 32];
    snprintf(left, sizeof(left), "*** %s left the chat ***\n", c->username);

    broadcast(clients, fd, left);
  } else {
    printf("Client %d disconnected.\n", fd);
  }

  close(fd);
  FD_CLR(fd, master_set);
  clients_delete(clients, fd);
}

void accept_client(int server_fd, Clients *clients, fd_set *master_set,
                   int *max_fd) {
  int client_fd = accept(server_fd, NULL, NULL);
  if (client_fd < 0) {
    perror("accept");
    return;
  }

  if (client_fd >= FD_SETSIZE) {
    fprintf(stderr, "Client descriptor exceeds select() capacity\n");
    close(client_fd);
    return;
  }

  printf("New client connected: fd %d\n", client_fd);
  Client c = {.fd = client_fd, .state = CLIENT_WAITING_USERNAME};

  if (clients_add(clients, c) == -1) {
    const char *msg = "Server is full, try again another time!\n";
    send(client_fd, msg, strlen(msg), 0);

    close(client_fd);
    return;
  }

  FD_SET(client_fd, master_set);
  if (client_fd > *max_fd)
    *max_fd = client_fd;

  const char *welcome = "Welcome to the server!\n";
  send(client_fd, welcome, strlen(welcome), 0);

  const char *ask_username = "Enter username: ";
  send(client_fd, ask_username, strlen(ask_username), 0);
}

void handle_client(Clients *clients, int fd, fd_set *master_set) {
  char buffer[MAX_BUFFER_SIZE];
  ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes <= 0) {
    if (bytes < 0)
      perror("recv");

    disconnect_client(clients, fd, master_set);
    return;
  }

  trim_newline(buffer, &bytes);

  Client *c = clients_search_fd(clients, fd);
  if (c == NULL) {
    fprintf(stderr, "Client with fd %d not found\n", fd);
    disconnect_client(clients, fd, master_set);
    return;
  }

  if (c->state == CLIENT_WAITING_USERNAME) {
    if (bytes == 0) {
      const char *msg = "Username cannot be empty.\n"
                        "Enter username: ";
      send(fd, msg, strlen(msg), 0);

      return;
    }

    strncpy(c->username, buffer, MAX_NAME - 1);

    c->username[MAX_NAME - 1] = '\0';
    c->state = CLIENT_CONNECTED;

    printf("Client fd %d chose username: %s\n", c->fd, c->username);

    const char *msg = "Username accepted!\n" PROMPT;
    send(fd, msg, strlen(msg), 0);

    char joined[MAX_NAME + 32];
    snprintf(joined, sizeof(joined), "*** %s joined the chat ***\n",
             c->username);
    broadcast(clients, fd, joined);

    return;
  }

  if (c->state == CLIENT_CONNECTED) {
    printf("Client %s sent: %s\n", c->username, buffer);

    // detect commands (start with '/')
    if (buffer[0] == '/') {
      if (handle_command(buffer + 1, fd, clients)) {
        disconnect_client(clients, fd, master_set);
      } else {
        send(fd, PROMPT, strlen(PROMPT), 0);
      }
      return;
    }

    char out[MAX_NAME + MAX_BUFFER_SIZE + 4];

    int len = prepare_broadcast_message(out, sizeof(out), c, buffer);
    if (len < 0) {
      fprintf(stderr, "Failed to prepare broadcast message\n");
      return;
    }

    broadcast(clients, fd, out);
    send(fd, PROMPT, strlen(PROMPT), 0);
  }
}

int main(void) {
  Clients clients = {.size = 0};

  int server_fd = create_server_socket();
  if (server_fd == -1)
    return 1;

  if (server_fd >= FD_SETSIZE) {
    fprintf(stderr, "Server descriptor exceeds select() capacity\n");
    close(server_fd);

    return 1;
  }

  printf("Listening on port %d...\n", PORT);

  fd_set master_set;
  fd_set read_set;

  FD_ZERO(&master_set);
  FD_SET(server_fd, &master_set);

  int max_fd = server_fd;

  while (1) {
    read_set = master_set;

    int ready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
    if (ready < 0) {
      perror("select");
      break;
    }

    for (int fd = 0; fd <= max_fd; fd++) {
      if (!FD_ISSET(fd, &read_set))
        continue;

      if (fd == server_fd)
        accept_client(server_fd, &clients, &master_set, &max_fd);
      else
        handle_client(&clients, fd, &master_set);
    }
  }

  return 0;
}
