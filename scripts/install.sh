#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
install_dir=${1:-/usr/local}

cd "$project_dir"
xmake f -c
xmake
xmake install -o "$install_dir"

printf '安装完成: %s/bin/trash\n' "$install_dir"
