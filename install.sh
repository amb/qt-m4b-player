#!/usr/bin/env bash
# Build and install the native release, including desktop integration.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./install.sh [--prefix DIRECTORY]

Build the native release and install it under ~/.local by default.
Use --prefix to choose another installation directory. This script does not
invoke sudo or install dependencies. DESTDIR is supported for staged installs.
EOF
}

prefix="${HOME}/.local"
while (( $# )); do
    case "$1" in
        --prefix)
            if (( $# < 2 )) || [[ -z "$2" || "$2" == -* ]]; then
                printf '%s\n' '--prefix requires a directory.' >&2
                exit 2
            fi
            prefix="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Resolve relative prefixes against the caller's working directory.
if [[ "$prefix" != /* ]]; then
    prefix="$PWD/$prefix"
fi

"$root/build.sh"
cmake --install "$root/build/release" --prefix "$prefix"

printf 'Installed under %s\n' "${DESTDIR:-}$prefix"
