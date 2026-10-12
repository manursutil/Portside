#ifndef CLIENTS_H
#define CLIENTS_H

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_NAME 32
#define MAX_CLIENTS 64
#define MAX_INCOMING_BUFFER 1024
#define MAX_OUTGOING_BUFFER 4096

typedef enum {
  CLIENT_WAITING_USERNAME,
  CLIENT_CONNECTED,
} ClientState;

typedef struct {
  int fd;
  char username[MAX_NAME];
  ClientState state;

  int disconnect_candidate;

  char incoming_buffer[MAX_INCOMING_BUFFER];
  size_t incoming_buffer_len;

  char outgoing_buffer[MAX_OUTGOING_BUFFER];
  size_t outgoing_buffer_len;
} Client;

typedef struct {
  Client items[MAX_CLIENTS];
  size_t size;
} Clients;

int clients_add(Clients *clients, Client client);
int clients_delete(Clients *clients, int fd);
Client *clients_search_fd(Clients *clients, int fd);
Client *clients_search_username(Clients *clients, char *username);
int client_send(Client *client, const char *data, size_t len);
int client_flush(Client *client);
int client_has_pending_data(Clients *clients, int fd);

#endif // !CLIENTS_H
