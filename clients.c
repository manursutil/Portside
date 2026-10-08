#include "clients.h"

// client structure
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
