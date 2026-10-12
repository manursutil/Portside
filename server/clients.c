#include "clients.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>

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

Client *clients_search_username(Clients *clients, char *username) {
  for (size_t i = 0; i < clients->size; i++) {
    if (strcmp(clients->items[i].username, username) == 0) {
      return &clients->items[i];
    }
  }

  return NULL;
}

int client_send(Client *client, const char *data, size_t len) {
  if (client->disconnect_candidate)
    return -1;

  if (len > sizeof(client->outgoing_buffer) - client->outgoing_buffer_len) {
    client->disconnect_candidate = 1;
    return -1;
  }

  if (len == 0)
    return 0;

  memcpy(client->outgoing_buffer + client->outgoing_buffer_len, data, len);
  client->outgoing_buffer_len += len;

  return 0;
}

int client_flush(Client *client) {
  size_t total = 0;

  while (total < client->outgoing_buffer_len) {
    ssize_t n = send(client->fd, client->outgoing_buffer + total,
                     client->outgoing_buffer_len - total, 0);

    if (n == -1) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      return -1;
    }

    if (n == 0)
      break;

    total += n;
  }

  if (total > 0) {
    memmove(client->outgoing_buffer, client->outgoing_buffer + total,
            client->outgoing_buffer_len - total);

    client->outgoing_buffer_len -= total;
  }

  return 0;
}

int client_has_pending_data(Clients *clients, int fd) {
  Client *c = clients_search_fd(clients, fd);
  if (c == NULL)
    return 0;

  return c->outgoing_buffer_len > 0;
}
