#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "clients.h"
#include "commands.h"
#include "net.h"
#include "protocol.h"

#define PORT 8080
#define MAX_MESSAGE 1024

void broadcast(Clients *clients, int sender_fd, const char *message) {
  size_t len = strlen(message);
  for (size_t i = 0; i < clients->size; i++) {
    Client *c = &clients->items[i];
    if (c->fd != sender_fd && c->state == CLIENT_CONNECTED) {
      client_send(c, message, len);
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
  Client c = {.fd = client_fd,
              .state = CLIENT_WAITING_USERNAME,
              .disconnect_candidate = 0,
              .incoming_buffer_len = 0,
              .outgoing_buffer_len = 0};

  if (clients_add(clients, c) == -1) {
    const char *msg = "Server is full, try again another time!\n";
    send_all(c.fd, msg, strlen(msg));

    close(client_fd);
    return;
  }

  int flags = fcntl(client_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    return;
  }

  if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    return;
  }

  FD_SET(client_fd, master_set);
  if (client_fd > *max_fd)
    *max_fd = client_fd;

  Client *cl = clients_search_fd(clients, c.fd);
  const char *welcome = "Welcome to the server!\n";
  client_send(cl, welcome, strlen(welcome));

  const char *ask_username = "Enter username: ";
  client_send(cl, ask_username, strlen(ask_username));
}

void process_message(Clients *clients, int fd, fd_set *master_set,
                     char *message) {
  Client *c = clients_search_fd(clients, fd);
  if (c == NULL)
    return;

  if (c->state == CLIENT_WAITING_USERNAME) {
    if (message[0] == '\0') {
      const char *msg = "Username cannot be empty.\n"
                        "Enter username: ";
      client_send(c, msg, strlen(msg));

      return;
    }

    strncpy(c->username, message, MAX_NAME - 1);

    c->username[MAX_NAME - 1] = '\0';
    c->state = CLIENT_CONNECTED;

    printf("Client fd %d chose username: %s\n", c->fd, c->username);

    const char *msg = "Username accepted!\n";
    client_send(c, msg, strlen(msg));

    char joined[MAX_NAME + 32];
    snprintf(joined, sizeof(joined), "*** %s joined the chat ***\n",
             c->username);
    broadcast(clients, fd, joined);

    return;
  }

  if (c->state == CLIENT_CONNECTED) {
    printf("Client %s sent: %s\n", c->username, message);

    // detect commands (start with '/')
    if (message[0] == '/') {
      if (handle_command(message + 1, fd, clients)) {
        disconnect_client(clients, fd, master_set);
      }
      return;
    }

    char out[MAX_NAME + MAX_MESSAGE + 4];

    int len = prepare_broadcast_message(out, sizeof(out), c, message);
    if (len < 0) {
      fprintf(stderr, "Failed to prepare broadcast message\n");
      return;
    }

    broadcast(clients, fd, out);
  }
}

void handle_client(Clients *clients, int fd, fd_set *master_set) {
  Client *c = clients_search_fd(clients, fd);
  if (c == NULL) {
    fprintf(stderr, "Client with fd %d not found\n", fd);
    disconnect_client(clients, fd, master_set);
    return;
  }

  char temp[MAX_INCOMING_BUFFER];

  ssize_t bytes = recv(fd, temp, sizeof(temp), 0);
  if (bytes <= 0) {
    if (bytes < 0)
      perror("recv");

    disconnect_client(clients, fd, master_set);
    return;
  }

  for (ssize_t i = 0; i < bytes; i++) {
    char ch = temp[i];

    if (ch == '\n') {
      if (c->incoming_buffer_len > 0 &&
          c->incoming_buffer[c->incoming_buffer_len - 1] == '\r') {
        c->incoming_buffer_len--;
      }

      c->incoming_buffer[c->incoming_buffer_len] = '\0';

      char message[MAX_INCOMING_BUFFER];
      memcpy(message, c->incoming_buffer, c->incoming_buffer_len + 1);

      process_message(clients, fd, master_set, message);

      c = clients_search_fd(clients, fd);
      if (c == NULL)
        return;

      // clear buffer after message has been processed
      // memset(c->buffer, 0, sizeof(c->buffer));
      c->incoming_buffer_len = 0;

    } else {
      if (c->incoming_buffer_len >= sizeof(c->incoming_buffer) - 1) {
        char *msg = "Message too long\n";
        client_send(c, msg, strlen(msg));
        c->disconnect_candidate = 1;

        // disconnect_client(clients, fd, master_set);
        return;
      }

      c->incoming_buffer[c->incoming_buffer_len++] = ch;
    }
  }
}

void cleanup(Clients *clients, fd_set *master_set) {
  // disconnect marked clients
  // clients_delete shifts elements, don't increase i automatically
  for (size_t i = 0; i < clients->size;) {
    if (clients->items[i].disconnect_candidate) {
      int fd = clients->items[i].fd;
      client_flush(&clients->items[i]);
      disconnect_client(clients, fd, master_set);
    } else {
      i++;
    }
  }
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);

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
  fd_set write_set;

  FD_ZERO(&master_set);
  FD_SET(server_fd, &master_set);

  int max_fd = server_fd;

  while (1) {
    read_set = master_set;
    FD_ZERO(&write_set);

    // Add clients with pending data to write set
    for (int fd = 0; fd <= max_fd; fd++) {
      if (fd == server_fd)
        continue;

      if (!FD_ISSET(fd, &master_set))
        continue;

      if (client_has_pending_data(&clients, fd)) {
        FD_SET(fd, &write_set);
      }
    }

    int ready = select(max_fd + 1, &read_set, &write_set, NULL, NULL);
    if (ready < 0) {
      perror("select");
      break;
    }

    for (int fd = 0; fd <= max_fd; fd++) {

      if (FD_ISSET(fd, &read_set)) {
        if (fd == server_fd)
          accept_client(server_fd, &clients, &master_set, &max_fd);
        else
          handle_client(&clients, fd, &master_set);
      }

      Client *client = clients_search_fd(&clients, fd);
      if (client != NULL && FD_ISSET(fd, &write_set)) {
        if (client_flush(client) == -1) {
          disconnect_client(&clients, fd, &master_set);
        }
      }
    }

    cleanup(&clients, &master_set);
  }

  return 0;
}
