#ifndef COMMANDS_H
#define COMMANDS_H

#include "clients.h"
#include <stdbool.h>

// Returns true when the client requested disconnection.
bool handle_command(char *cmd, int fd, Clients *clients);

#endif // !COMMANDS_H
