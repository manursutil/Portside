#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024
#define MAX_NAME 32
#define MAX_CLIENTS 16

// telnet adds \r\n to each line sent
// trim these newline characters for strcmp to work as expected
void trim_newline(char *buffer, ssize_t *bytes) {
  while (*bytes > 0 &&
         (buffer[*bytes - 1] == '\n' || buffer[*bytes - 1] == '\r')) {
    (*bytes)--;
  }

  buffer[*bytes] = '\0';
}

// client structure
typedef enum {
  CLIENT_WAITING_USERNAME,
  CLIENT_CONNECTED,
} ClientState;

typedef struct {
  int fd;
  char username[MAX_NAME];
  ClientState state;
} Client;

typedef struct {
  Client items[MAX_CLIENTS];
  size_t size;
} Clients;

int clients_add(Clients *clients, Client client) {
  if (clients->size >= MAX_CLIENTS)
    return -1;

  clients->items[clients->size] = client;
  clients->size++;

  return 0;
}

int clients_delete(Clients *clients, int fd) {
  for (size_t i = 0; i < clients->size; i++) {
    if (clients->items[i].fd == fd) {
      // Move everything after this client one position left
      for (size_t j = i; j + 1 < clients->size; j++) {
        clients->items[j] = clients->items[j + 1];
      }

      clients->size--;
      return 0;
    }
  }

  return -1;
}

// linear search for clients
Client *clients_search_fd(Clients *clients, int fd) {
  for (size_t i = 0; i < clients->size; i++) {
    if (clients->items[i].fd == fd) {
      return &clients->items[i];
    }
  }

  return NULL;
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

int main(void) {
  Clients clients = {.size = 0};

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket");
    return -1;
  }

  // initialize the socket address struct
  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));

  address.sin_family = AF_INET;         // IPv4
  address.sin_addr.s_addr = INADDR_ANY; // any local interface
  address.sin_port = htons(PORT);       // port

  // bind to the same port after restarting the server
  int yes = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  // bind socket to a port
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  // listen to connections
  if (listen(server_fd, 10) < 0) {
    perror("listen");
    return 1;
  }

  printf("Listening on port %d...\n", PORT);

  // prepare select()
  fd_set master_set; // all connections
  fd_set read_set;   // connections that are ready right now

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

    // event loop:
    // check all descriptors
    for (int fd = 0; fd <= max_fd; fd++) {
      if (!FD_ISSET(fd, &read_set))
        continue;

      if (fd == server_fd) {
        // accept new user
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
          perror("accept");
          continue;
        }

        printf("New client connected: fd %d\n", client_fd);
        FD_SET(client_fd, &master_set);

        if (client_fd > max_fd)
          max_fd = client_fd;

        // create client
        Client c;
        c.fd = client_fd;
        c.username[0] = '\0';
        c.state = CLIENT_WAITING_USERNAME;

        // append client to array
        if (clients_add(&clients, c) == -1) {
          // server is full
          const char *server_full_msg =
              "Server is full, try again another time!\n";

          send(client_fd, server_full_msg, strlen(server_full_msg), 0);

          close(client_fd);
          FD_CLR(client_fd, &master_set);
          continue;
        }

        // welcome message and ask for username
        const char *welcome = "Welcome to the server!\n";
        send(client_fd, welcome, strlen(welcome), 0);

        const char *ask_username = "Enter username: ";
        send(client_fd, ask_username, strlen(ask_username), 0);

      } else {
        // handles an existing client

        // receive message
        char buffer[MAX_BUFFER_SIZE]; // buffer to store data
        ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0); // receive

        if (bytes > 0) {
          buffer[bytes] = '\0';
          trim_newline(buffer, &bytes);

          Client *c = clients_search_fd(&clients, fd);

          if (c == NULL) {
            fprintf(stderr, "Client with fd %d not found\n", fd);

            close(fd);
            FD_CLR(fd, &master_set);
            continue;
          }

          // client is waiting to choose a username
          if (c->state == CLIENT_WAITING_USERNAME) {
            if (bytes == 0) {
              const char *msg = "Username cannot be empty.\n"
                                "Enter username: ";

              send(fd, msg, strlen(msg), 0);
              continue;
            }

            // prepare client username
            strncpy(c->username, buffer, MAX_NAME - 1);
            c->username[MAX_NAME - 1] = '\0';

            c->state = CLIENT_CONNECTED;

            printf("Client fd %d chose username: %s\n", c->fd, c->username);

            const char *msg = "Username accepted!\n";
            send(fd, msg, strlen(msg), 0);

            continue;
          }

          // client is fully connected and can send chat messages
          if (c->state == CLIENT_CONNECTED) {
            printf("Client %s sent: %s\n", c->username, buffer);

            if (strcmp(buffer, "quit") == 0) {
              printf("Client %s disconnected.\n", c->username);

              close(c->fd); // close the fd and clear from master
              FD_CLR(c->fd, &master_set);
              clients_delete(&clients, fd);

              continue;
            }

            const char *response = "Message received!\n";
            send(fd, response, strlen(response), 0);

            // echo messages to all clients
            char out[MAX_NAME + MAX_BUFFER_SIZE + 4];

            int len = prepare_broadcast_message(out, sizeof(out), c, buffer);

            if (len < 0) {
              fprintf(stderr, "Failed to prepare broadcast message\n");
              continue;
            }

            for (int i = 0; i <= max_fd; i++) {
              if (i == server_fd || i == fd)
                continue;

              if (!FD_ISSET(i, &master_set))
                continue;

              Client *other = clients_search_fd(&clients, i);

              if (other == NULL)
                continue;

              // only send messages to clients that have chosen a username
              if (other->state != CLIENT_CONNECTED)
                continue;

              send(i, out, len, 0);
            }
          }

        } else if (bytes == 0) {
          Client *c = clients_search_fd(&clients, fd);

          if (c != NULL && c->state == CLIENT_CONNECTED)
            printf("Client %s disconnected.\n", c->username);
          else
            printf("Client %d disconnected.\n", fd);

          close(fd);
          FD_CLR(fd, &master_set);
          clients_delete(&clients, fd);

        } else {
          perror("recv");

          close(fd);
          FD_CLR(fd, &master_set);
          clients_delete(&clients, fd);
        }
      }
    }
  }

  return 0;
}
