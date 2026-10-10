# Portside

A small multi-user TCP chat server written in C. Built to learn POSIX sockets, `select()`, and client state management on Linux or macOS.

## Build and run

You need Bash, GCC or Clang, and Netcat (`nc`) or Telnet.

```sh
./build.sh
./bin/main
```

The server listens on port **8080** on all IPv4 interfaces. To use Clang, build with `CC=clang ./build.sh`.

Open two or more terminals and connect from each:

```sh
nc localhost 8080
```

You can also use `telnet localhost 8080`, though full Telnet negotiation is not implemented.

Enter a username when prompted, then type messages. Other connected users see `username: message`, along with join and leave notices. Type `quit` to disconnect, or press Ctrl+C in the server terminal to stop it.

The server accepts up to 64 clients. Usernames are limited to 31 bytes; longer names are truncated.

Other build commands:

```sh
./build.sh release  # Optimized build
./build.sh clean    # Remove build output
```

## What's next

- Buffer incoming data per client and process complete lines, including split messages and multiple lines in one read.
- Add `/quit`, `/help`, `/users`, `/name`, and `/msg` commands.
- Reject duplicate or oversized usernames, oversized messages, and malformed commands.
- Handle partial sends, send errors, and `SIGPIPE` when a peer disconnects.
- Build a C client that uses `select()` to read keyboard input and server messages.
- Improve error handling and shutdown cleanup, then explore `poll()`.

Later ideas: rooms, timestamps, chat history, IPv6, and TLS through an established library.

This is a learning project with plaintext traffic and no authentication. Complete-line buffering and reliable sending are still pending.
