# Portside

A small multi-user TCP chat server and terminal client written in C. Built to learn POSIX sockets, `select()`, and client state management on Linux or macOS.

> **Learning project.** Traffic is plaintext and there is no authentication. Don't expose it to untrusted networks.

## Features

- Many clients at once on a single thread, using `select()` for I/O multiplexing
- Username prompt on connect, with join and leave notices sent to everyone else
- Slash commands for help, listing users, renaming, private messages, and quitting
- Reliable sending: `send_all()` retries partial writes, and `SIGPIPE` is ignored so a client that drops mid-send can't crash the server
- A C client that uses `select()` to watch the keyboard and the server at the same time and draws its own `> ` prompt
- The server also works with any line-based TCP client, such as `nc` or `telnet`
- No dependencies beyond the C standard library and POSIX

## Requirements

- Linux or macOS
- Bash
- GCC or Clang
- Optional: Netcat (`nc`) or Telnet as an alternative client

## Getting started

Build and start the server:

```sh
./build.sh
./bin/server
```

The server listens on port **8080** on all IPv4 interfaces.

Open two or more terminals and run the client in each:

```sh
./bin/client
```

The client connects to `127.0.0.1:8080`. Type a line and press Enter to send it; press Ctrl+D to quit.

You can also connect with `nc localhost 8080` or `telnet localhost 8080`, though Telnet option negotiation isn't implemented.

Enter a username when prompted, then type messages. Other users see `username: message`. Press Ctrl+C in the server terminal to stop it.

`./build.sh` produces both `bin/server` and `bin/client`.

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
| Port | 8080 | `server/main.c` and `client/main.c` (`PORT`) |
| Client server address | 127.0.0.1 | `client/main.c` (`SERVER_IP`) |
| Max clients | 64 | `server/clients.h` (`MAX_CLIENTS`) |
| Username length | 31 bytes (longer names are truncated) | `server/clients.h` (`MAX_NAME`) |
| Read buffer | 1024 bytes | `server/main.c` and `client/main.c` (`MAX_MESSAGE`) |

When the server is full, new connections get a short message and are closed.

## Project structure

```
.
├── common/
│   └── net.c/.h      # Shared by server and client: send_all()
├── server/
│   ├── main.c        # Server socket, select() loop, accept/read/disconnect, broadcast
│   ├── clients.c/.h  # Fixed-size client table: add, delete, look up by fd or username
│   ├── commands.c/.h # Slash-command parsing and handlers
│   └── protocol.c/.h # Broadcast message formatting
├── client/
│   └── main.c        # Terminal client: select() over stdin and the socket
├── tests/msg.c       # Tests for /msg over socketpairs
├── build.sh          # Build script (debug, release, clean)
└── .clangd           # Include paths for clangd
```

## How it works

1. `main()` opens a listening IPv4 socket and adds it to a `select()` set.
2. When the listening socket is readable, the server accepts a new client, stores it in the client table in the `CLIENT_WAITING_USERNAME` state, and asks for a username.
3. When a client socket is readable, the server reads one chunk. The first line becomes the username and moves the client to `CLIENT_CONNECTED`. After that, each line is either a command (`/...`) or a chat message broadcast to every other connected client.
4. When `recv()` returns 0 or fails, the client is removed and a leave notice is broadcast.

The client connects, then loops on `select()` over stdin and the socket. Each line typed is sent with `send_all()`; each chunk received is printed after a carriage return, and the `> ` prompt is redrawn once a full line has arrived. It exits when the server closes the connection or stdin hits EOF.

## Testing

`tests/msg.c` tests the `/msg` handler without a network, using `socketpair()`. It takes the command text as an argument:

```sh
./build.sh
cc -Wall -Wextra -Icommon -Iserver tests/msg.c common/net.c server/commands.c server/clients.c -o bin/test_msg
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
- [x] C terminal client using `select()` over stdin and the socket
- [ ] Client: keep a partially typed line intact when a message arrives, and accept host/port arguments
- [X] Per-client buffering of complete lines, including messages split across reads and several lines in one read
- [ ] Reject duplicate or oversized usernames, oversized messages, and malformed commands
- [ ] Better error handling and clean shutdown, then try `poll()`

Later ideas: rooms, timestamps, chat history, IPv6, and TLS through an established library.

## Known limitations

- Usernames aren't checked for uniqueness, and `/name` doesn't notify other users.
- The client has no line editing: an incoming message overwrites the prompt line, so text you've half-typed disappears from view (it's still sent when you press Enter).
- The client always connects to `127.0.0.1:8080`.
- IPv4 only. `select()` limits descriptors to `FD_SETSIZE`.

This is a learning project, not production software. Traffic is plaintext, there's no authentication, and complete-line buffering is still pending.
