#ifndef CLIENTS_H
#define CLIENTS_H

#include <stddef.h>

#define MAX_NAME 32
#define MAX_CLIENTS 64
#define MAX_BUFFER 1024

typedef enum {
  CLIENT_WAITING_USERNAME,
  CLIENT_CONNECTED,
} ClientState;

typedef struct {
  int fd;
  char username[MAX_NAME];
  ClientState state;

  char buffer[MAX_BUFFER];
  size_t buffer_len;
} Client;

typedef struct {
  Client items[MAX_CLIENTS];
  size_t size;
} Clients;

int clients_add(Clients *clients, Client client);
int clients_delete(Clients *clients, int fd);
Client *clients_search_fd(Clients *clients, int fd);
Client *clients_search_username(Clients *clients, char *username);

#endif // !CLIENTS_H
