set -e
mkdir -p bin/
gcc -Wall -Wextra -g main.c clients.c protocol.c -o bin/main
