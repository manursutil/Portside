# Portside

A small multi-user TCP chat server written in C. Built to learn POSIX sockets, `select()`, and client state management on Linux or macOS.

> **Learning project.** Traffic is plaintext and there is no authentication. Don't expose it to untrusted networks.

## Features

- Many clients at once on a single thread, using `select()` for I/O multiplexing
- Username prompt on connect, with join and leave notices sent to everyone else
- Slash commands for help, listing users, renaming, private messages, and quitting
- Reliable sending: `send_all()` retries partial writes, and `SIGPIPE` is ignored so a client that drops mid-send can't crash the server
- Works with any line-based TCP client, such as `nc` or `telnet`
- No dependencies beyond the C standard library and POSIX

## Requirements

- Linux or macOS
- Bash
- GCC or Clang
- Netcat (`nc`) or Telnet to connect

## Getting started

Build and start the server:

```sh
./build.sh
./bin/main
```

The server listens on port **8080** on all IPv4 interfaces.

Open two or more terminals and connect from each:

```sh
nc localhost 8080
```

`telnet localhost 8080` also works, though Telnet option negotiation isn't implemented.

Enter a username when prompted, then type messages. Other users see `username: message`. Press Ctrl+C in the server terminal to stop it.

### Build options

```sh
./build.sh          # Debug build (-g -O0), the default
./build.sh release  # Optimized build (-O2 -DNDEBUG)
./build.sh clean    # Remove bin/
CC=clang ./build.sh # Use a different compiler
```

## Commands

Any line that starts with `/` is read as a command.

| Command | Description |
|---|---|
| `/help` | List the available commands |
| `/users` | List everyone connected to the server |
| `/name <username>` | Change your username |
| `/msg <username> <message>` | Send a private message to one user |
| `/quit` | Disconnect from the server |

## Limits

These limits are compile-time constants:

| Setting | Value | Defined in |
|---|---|---|
| Port | 8080 | `main.c` (`PORT`) |
| Max clients | 64 | `clients.h` (`MAX_CLIENTS`) |
| Username length | 31 bytes (longer names are truncated) | `clients.h` (`MAX_NAME`) |
| Read buffer | 1024 bytes | `main.c` (`MAX_MESSAGE`) |

When the server is full, new connections get a short message and are closed.

## Project structure

```
.
├── main.c        # Server socket, select() loop, accept/read/disconnect, broadcast
├── clients.c/.h  # Fixed-size client table: add, delete, look up by fd or username
├── commands.c/.h # Slash-command parsing and handlers
├── protocol.c/.h # Line trimming, message formatting, send_all()
├── tests/msg.c   # Tests for /msg over socketpairs
└── build.sh      # Build script (debug, release, clean)
```

## How it works

1. `main()` opens a listening IPv4 socket and adds it to a `select()` set.
2. When the listening socket is readable, the server accepts a new client, stores it in the client table in the `CLIENT_WAITING_USERNAME` state, and asks for a username.
3. When a client socket is readable, the server reads one chunk. The first line becomes the username and moves the client to `CLIENT_CONNECTED`. After that, each line is either a command (`/...`) or a chat message broadcast to every other connected client.
4. When `recv()` returns 0 or fails, the client is removed and a leave notice is broadcast.

## Testing

`tests/msg.c` tests the `/msg` handler without a network, using `socketpair()`. It takes the command text as an argument:

```sh
./build.sh
cc -Wall -Wextra -I. tests/msg.c commands.c clients.c protocol.c -o bin/test_msg
./bin/test_msg "msg bob hello"     # Delivered
./bin/test_msg "msg nobody hello"  # Unknown user
./bin/test_msg "msg bob"           # Usage error
```

Each run prints `PASS`, or exits early on a failed assertion.

## Roadmap

- [x] Multi-client server with `select()`
- [x] Username prompt and join/leave notices
- [x] `/help`, `/users`, `/name`, `/msg`, `/quit`
- [x] Partial sends, send errors, and `SIGPIPE`
- [ ] **C client (coming soon).** A terminal client that uses `select()` to read keyboard input and server messages at the same time, so incoming messages don't interrupt what you're typing.
- [ ] Per-client buffering of complete lines, including messages split across reads and several lines in one read
- [ ] Reject duplicate or oversized usernames, oversized messages, and malformed commands
- [ ] Better error handling and clean shutdown, then try `poll()`

Later ideas: rooms, timestamps, chat history, IPv6, and TLS through an established library.

## Known limitations

- Each `recv()` is treated as one message, so input isn't split on line boundaries yet.
- Usernames aren't checked for uniqueness, and `/name` doesn't notify other users.
- IPv4 only. `select()` limits descriptors to `FD_SETSIZE`.

This is a learning project, not production software. Traffic is plaintext, there's no authentication, and complete-line buffering is still pending.
