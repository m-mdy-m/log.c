#!/usr/bin/env bash
# Build and execute a brand-new external consumer against an installed prefix.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 INSTALL_PREFIX [CC]" >&2
  exit 2
fi
prefix="$(cd "$1" && pwd -P)"
cc="${2:-${CC:-cc}}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
include_dir="$prefix/include"
lib_dir="$prefix/lib"
test_source="$repo_root/tests/e2e_consumer.c"

[[ -f "$include_dir/m-mdy-m/log.h" ]] || { echo "Missing installed public header: $include_dir/m-mdy-m/log.h" >&2; exit 1; }
[[ -f "$test_source" ]] || { echo "Missing E2E consumer source" >&2; exit 1; }
tmp="$(mktemp -d "${TMPDIR:-/tmp}/m-mdy-m-log-e2e.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

static_lib="$lib_dir/libm-mdy-m-log.a"
[[ -f "$static_lib" ]] || { echo "Missing installed static library: $static_lib" >&2; exit 1; }
"$cc" -std=c11 -Wall -Wextra -Wpedantic -I"$include_dir" "$test_source" "$static_lib" -o "$tmp/consumer-static"
"$tmp/consumer-static"

platform="$(uname -s 2>/dev/null || printf unknown)"
if [[ "$platform" == MINGW* || "$platform" == MSYS* || "$platform" == CYGWIN* ]]; then
  import_lib="$lib_dir/libm-mdy-m-log.dll.a"
  dll="$prefix/bin/m-mdy-m-log.dll"
  [[ -f "$dll" ]] || dll="$prefix/bin/libm-mdy-m-log.dll"
  if [[ -f "$import_lib" && -f "$dll" ]]; then
    "$cc" -std=c11 -Wall -Wextra -Wpedantic -I"$include_dir" "$test_source" \
      -L"$lib_dir" -lm-mdy-m-log -o "$tmp/consumer-shared.exe"
    cp "$dll" "$tmp/"
    (cd "$tmp" && PATH="$tmp:$PATH" ./consumer-shared.exe)
  else
    echo "Installed shared library/import library not found; shared E2E is required." >&2
    exit 1
  fi
elif [[ -e "$lib_dir/libm-mdy-m-log.so" || -e "$lib_dir/libm-mdy-m-log.so.1" ]]; then
  "$cc" -std=c11 -Wall -Wextra -Wpedantic -I"$include_dir" "$test_source" \
    -L"$lib_dir" -Wl,-rpath,"$lib_dir" -lm-mdy-m-log -o "$tmp/consumer-shared"
  LD_LIBRARY_PATH="$lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$tmp/consumer-shared"
elif [[ -e "$lib_dir/libm-mdy-m-log.dylib" || -e "$lib_dir/libm-mdy-m-log.1.dylib" ]]; then
  "$cc" -std=c11 -Wall -Wextra -Wpedantic -I"$include_dir" "$test_source" \
    -L"$lib_dir" -Wl,-rpath,"$lib_dir" -lm-mdy-m-log -o "$tmp/consumer-shared"
  DYLD_LIBRARY_PATH="$lib_dir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$tmp/consumer-shared"
else
  echo "No installed shared library detected; shared E2E is required." >&2
  exit 1
fi
