#!/usr/bin/env bash
# Configure/test static and shared builds, install both to an isolated prefix,
# then compile and execute a consumer that sees only the installed SDK.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
STAGE_ARG="${1:-$ROOT/build/stage}"
CC_BIN="${CC:-cc}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"
PROJECT_VERSION="$(sed -n 's/^project(m_mdy_m_log VERSION \([^ ]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
MAKE_VERSION="$(sed -n 's/^VERSION := //p' "$ROOT/Makefile")"
[[ -n "$PROJECT_VERSION" && "$PROJECT_VERSION" == "$MAKE_VERSION" ]] || {
  echo "Version mismatch: CMake=$PROJECT_VERSION Make=$MAKE_VERSION" >&2
  exit 1
}
mkdir -p "$(dirname "$STAGE_ARG")"
STAGE="$(cd "$(dirname "$STAGE_ARG")" && pwd -P)/$(basename "$STAGE_ARG")"
rm -rf "$STAGE" "$ROOT/build-static" "$ROOT/build-shared"

for kind in static shared; do
  shared=OFF
  [[ "$kind" == shared ]] && shared=ON
  build="$ROOT/build-$kind"
  cmake -S "$ROOT" -B "$build" -G "$GENERATOR" \
    -DCMAKE_C_COMPILER="$CC_BIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS="$shared"
  cmake --build "$build" --config Release --parallel
  ctest --test-dir "$build" -C Release --output-on-failure
  cmake --install "$build" --prefix "$STAGE"
done

CC="$CC_BIN" bash "$ROOT/tests/e2e_install.sh" "$STAGE" "$CC_BIN"
cmake -S "$ROOT/tests/e2e-cmake" -B "$ROOT/build/e2e-cmake" \
  -G "$GENERATOR" -DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$STAGE"
cmake --build "$ROOT/build/e2e-cmake" --config Release --parallel
ctest --test-dir "$ROOT/build/e2e-cmake" -C Release --output-on-failure

# Verify the actual binary release archive and its installer, not just the raw CMake prefix.
PACKAGE_ROOT="$ROOT/build/e2e-package"
rm -rf "$PACKAGE_ROOT"
python3 "$ROOT/tools/package_binary.py" --prefix "$STAGE" --version "$PROJECT_VERSION" \
  --platform e2e --format tar.gz --outdir "$PACKAGE_ROOT/dist"
mkdir -p "$PACKAGE_ROOT/extract"
tar -xzf "$PACKAGE_ROOT/dist/m-mdy-m-log-v${PROJECT_VERSION}-e2e.tar.gz" -C "$PACKAGE_ROOT/extract"
BUNDLE="$PACKAGE_ROOT/extract/m-mdy-m-log-v${PROJECT_VERSION}-e2e"
bash "$BUNDLE/install.sh" --prefix "$PACKAGE_ROOT/installed"
CC="$CC_BIN" bash "$ROOT/tests/e2e_install.sh" "$PACKAGE_ROOT/installed" "$CC_BIN"
cmake -S "$ROOT/tests/e2e-cmake" -B "$ROOT/build/e2e-package-cmake" \
  -G "$GENERATOR" -DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PACKAGE_ROOT/installed"
cmake --build "$ROOT/build/e2e-package-cmake" --config Release --parallel
ctest --test-dir "$ROOT/build/e2e-package-cmake" -C Release --output-on-failure
python3 "$ROOT/tests/test_check_abi_parser.py"
python3 "$ROOT/tools/check_abi.py"
python3 "$ROOT/tools/check_docs.py"
if [[ "$(uname -s 2>/dev/null || printf unknown)" != MINGW* && "$(uname -s 2>/dev/null || printf unknown)" != MSYS* ]] && command -v pkg-config >/dev/null 2>&1; then
  version="$(PKG_CONFIG_PATH="$STAGE/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" pkg-config --modversion m-mdy-m-log)"
  [[ "$version" == "$PROJECT_VERSION" ]] || { echo "Unexpected installed pkg-config version: $version (expected $PROJECT_VERSION)" >&2; exit 1; }
  echo "installed pkg-config: PASS ($version)"
fi
printf 'Build/test/install/package-installer/CMake-consumer E2E complete: %s\n' "$STAGE"
