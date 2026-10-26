#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: ./install.sh [--prefix PATH]

Install the m-mdy-m/log.c headers and native libraries from this release bundle.
The default prefix is ~/.local; use --prefix /usr/local for a system install.
USAGE
}

prefix="${HOME:?HOME must be set}/.local"
while (($#)); do
  case "$1" in
    --prefix|-p)
      [[ $# -ge 2 && -n "$2" ]] || { echo "--prefix requires a path" >&2; exit 2; }
      prefix="$2"
      shift 2
      ;;
    --help|-h) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
for required in "$root/include/m-mdy-m/log.h" "$root/lib/libm-mdy-m-log.a" "$root/lib/pkgconfig/m-mdy-m-log.pc"; do
  [[ -f "$required" ]] || { echo "Incomplete release archive; missing $required" >&2; exit 1; }
done
prefix="$(mkdir -p "$prefix" && cd "$prefix" && pwd -P)"
for dir in include lib bin share; do
  [[ -d "$root/$dir" ]] || continue
  mkdir -p "$prefix/$dir"
  cp -a "$root/$dir/." "$prefix/$dir/"
done
printf 'Installed m-mdy-m/log.c to %s\n' "$prefix"
printf 'Header: %s/include/m-mdy-m/log.h\n' "$prefix"
printf 'Static library: %s/lib/libm-mdy-m-log.a\n' "$prefix"
printf 'pkg-config: export PKG_CONFIG_PATH="%s/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"\n' "$prefix"
printf 'Link applications with: pkg-config --cflags --libs m-mdy-m-log\n'
