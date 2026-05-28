#!/usr/bin/env sh
set -eu

install_dir=${1:-/usr/local}
repo_archive_url=${TRASH_ARCHIVE_URL:-https://github.com/adddaao/trash/archive/refs/heads/master.tar.gz}

project_dir=""
script_path=${0:-}

if [ -n "$script_path" ] && [ -f "$script_path" ]; then
    script_dir=$(CDPATH= cd -- "$(dirname -- "$script_path")" && pwd)
    candidate_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
    if [ -f "$candidate_dir/xmake.lua" ]; then
        project_dir=$candidate_dir
    fi
fi

if [ -z "$project_dir" ]; then
    if ! command -v curl >/dev/null 2>&1; then
        printf 'error: curl is required for remote install\n' >&2
        exit 1
    fi
    if ! command -v tar >/dev/null 2>&1; then
        printf 'error: tar is required for remote install\n' >&2
        exit 1
    fi

    work_parent=${TMPDIR:-/tmp}
    work_dir="$work_parent/trash-install-$$"
    archive_file="$work_dir/source.tar.gz"
    mkdir -p "$work_dir"

    printf 'Downloading source: %s\n' "$repo_archive_url"
    curl -fsSL "$repo_archive_url" -o "$archive_file"
    tar -xzf "$archive_file" -C "$work_dir"

    for candidate_dir in "$work_dir"/*; do
        if [ -f "$candidate_dir/xmake.lua" ]; then
            project_dir=$candidate_dir
            break
        fi
    done

    if [ -z "$project_dir" ]; then
        printf 'error: downloaded archive does not contain xmake.lua\n' >&2
        exit 1
    fi
fi

if ! command -v xmake >/dev/null 2>&1; then
    printf 'error: xmake is required. Install xmake first, then rerun this script.\n' >&2
    exit 1
fi

cd "$project_dir"
xmake f -c
xmake
xmake install -o "$install_dir"

printf 'Installed: %s/bin/trash\n' "$install_dir"
