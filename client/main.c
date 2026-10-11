#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net.h"

#define PORT 8080
#define MAX_MESSAGE 1024
#define SERVER_IP "127.0.0.1"

int create_client_socket(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr) <= 0) {
    perror("inet_pton");
    close(fd);
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
    perror("connect");
    close(fd);
    return -1;
  }

  return fd;
}

int handle_stdin(const int client_fd, char *buffer, size_t buffer_size) {
  if (fgets(buffer, buffer_size, stdin) == NULL) {
    if (ferror(stdin))
      perror("fgets");
    return -1;
  }

  if (send_all(client_fd, buffer, strlen(buffer)) < 0)
    return -1;

  return 0;
}

int handle_client_socket(const int client_fd, char *buffer,
                         size_t buffer_size) {
  ssize_t n = recv(client_fd, buffer, buffer_size - 1, 0);
  if (n < 0) {
    perror("recv");
    return -1;
  }

  if (n == 0) {
    printf("Server disconnected.\n");
    return -1;
  }

  buffer[n] = '\0';
  printf("%s", buffer);
  fflush(stdout);

  return buffer[n - 1] == '\n';
}

int main(void) {
  int client_fd = create_client_socket();
  if (client_fd == -1)
    return 1;

  char buffer[MAX_MESSAGE];

  fd_set read_fds;

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(client_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    int ready = select(client_fd + 1, &read_fds, NULL, NULL, NULL);
    if (ready < 0) {
      perror("select");
      break;
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      // stdin has data available to read
      // read stdin and send all
      if (handle_stdin(client_fd, buffer, sizeof(buffer)) < 0)
        break;
      printf("> ");
      fflush(stdout);
    }

    if (FD_ISSET(client_fd, &read_fds)) {
      // client socket has data available to read
      // recv
      printf("\r");

      int result = handle_client_socket(client_fd, buffer, sizeof(buffer));
      if (result < 0)
        break;

      if (result == 1) {
        printf("> ");
        fflush(stdout);
      }
    }
  }

  close(client_fd);
  return 0;
}
