#!/usr/bin/env bash
set -eu

CC="${CC:-gcc}"
OUT="bin/main"
SRC=( ./*.c )

case "${1:-debug}" in
    debug)
        FLAGS="-Wall -Wextra -g -O0"
        ;;
    release)
        FLAGS="-Wall -Wextra -O2 -DNDEBUG"
        ;;
    clean)
        rm -rf bin/
        exit 0
        ;;
    *)
        echo "Usage: $0 [debug|release|clean]"
        exit 1
        ;;
esac

mkdir -p bin/
$CC $FLAGS "${SRC[@]}" -o "$OUT"

echo "Built $OUT"

