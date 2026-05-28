#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)

cd "$project_dir"

if command -v xmake >/dev/null 2>&1; then
    xmake f -p linux -c
    xmake -vD
else
    out_file="${TMPDIR:-/tmp}/trash_wsl.o"
    g++ -std=c++11 -Wall -Wextra -c src/wsl/trash_wsl.cpp -o "$out_file"
    printf 'xmake not found; checked WSL source with g++: %s\n' "$out_file"
fi
