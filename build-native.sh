#!/usr/bin/env bash
# build-native.sh — compile the DRM wrapper as a host-native glibc x86-64 binary.
#
# Prerequisites:
#   - libhybris already built: /tmp/hybris-x86_64-build/libhybris-core.so
#   - linker plugin: /tmp/hybris-linker/q.so
#   - Android rootfs: drm/rootfs/system/lib64/ (relative to the deploy directory)
#
# Environment expected at runtime:
#   HYBRIS_LINKER_DIR=/tmp/hybris-linker
#   HYBRIS_LD_LIBRARY_PATH=<path to rootfs/system/lib64>
#   HYBRIS_ANDROID_LIB64=<path to rootfs/system/lib64>

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD=/tmp/wrapper-native
# Directory holding libhybris-core.so; override to link against another copy
# (e.g. the one deployed in $DEPLOY_DIR).
HYBRIS_BUILD="${HYBRIS_BUILD:-/tmp/hybris-x86_64-build}"
HYBRIS_INC="${HYBRIS_INC:?Set HYBRIS_INC to the libhybris include directory}"
DOBBY_SRC="${DOBBY_SRC:-/tmp/dobby-src}"
DOBBY_BUILD="${DOBBY_BUILD:-/tmp/dobby-build}"
CJSON_DIR="$BUILD/cjson"

echo "=== host-native DRM wrapper ==="
mkdir -p "$BUILD" "$CJSON_DIR"

# ── Fetch cJSON (single .c + .h) ─────────────────────────────────────────────
if [[ ! -f "$CJSON_DIR/cJSON.h" ]]; then
    echo "--- fetching cJSON v1.7.19 ---"
    curl -fsSL "https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.19/cJSON.c" -o "$CJSON_DIR/cJSON.c"
    curl -fsSL "https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.19/cJSON.h" -o "$CJSON_DIR/cJSON.h"
fi

CFLAGS=(
    -O2
    -Wall
    -DMyRelease           # disables curl/log debug hooks only; the Dobby R1 hook stays enabled
    -D_GNU_SOURCE
    -include sys/time.h   # gettimeofday — not explicitly included in main.c, provided by NDK headers
    -I"$HERE"             # import.h, cmdline.h
    -I"$CJSON_DIR"        # cJSON.h
    -I"$DOBBY_SRC/include"      # dobby.h
    -I"$HYBRIS_INC"       # hybris/android/dlopen.h etc. (for reference, not required)
)

# ── Compile C sources ─────────────────────────────────────────────────────────
echo "--- compiling main.c ---"
gcc "${CFLAGS[@]}" -fPIC -c "$HERE/main.c" -o "$BUILD/main.o"

echo "--- compiling cmdline.c ---"
gcc "${CFLAGS[@]}" -fPIC -c "$HERE/cmdline.c" -o "$BUILD/cmdline.o"

echo "--- compiling hybris_stubs.c ---"
gcc "${CFLAGS[@]}" -fPIC -c "$HERE/hybris_stubs.c" -o "$BUILD/hybris_stubs.o"

echo "--- compiling hybris_ctor.c ---"
gcc "${CFLAGS[@]}" -fPIC -c "$HERE/hybris_ctor.c" -o "$BUILD/hybris_ctor.o"

echo "--- compiling cJSON.c ---"
gcc -O2 -fPIC -c "$CJSON_DIR/cJSON.c" -o "$BUILD/cjson.o"

# ── Compile C++ source ────────────────────────────────────────────────────────
echo "--- compiling main.cpp ---"
g++ -std=c++17 "${CFLAGS[@]}" -fPIC -c "$HERE/main.cpp" -o "$BUILD/main_cpp.o"

# ── Link binary ───────────────────────────────────────────────────────────────
echo "--- linking drm-native ---"
g++ \
    "$BUILD/main.o" \
    "$BUILD/cmdline.o" \
    "$BUILD/hybris_stubs.o" \
    "$BUILD/hybris_ctor.o" \
    "$BUILD/cjson.o" \
    "$BUILD/main_cpp.o" \
    "$DOBBY_BUILD/libdobby.a" \
    -L"$HYBRIS_BUILD" -lhybris-core \
    -lcurl \
    -lpthread \
    -ldl \
    -lm \
    -Wl,-rpath,"$HYBRIS_BUILD" \
    -o "$BUILD/drm-native"

# ── Compile drm_lib.c for the shared library ──────────────────────────────────
echo "--- compiling drm_lib.c ---"
gcc "${CFLAGS[@]}" -fPIC -DDRM_LIB_BUILD -c "$HERE/drm_lib.c" -o "$BUILD/drm_lib.o"

# Re-compile main.c with -DDRM_LIB_BUILD (guards out int main()) for the .so
echo "--- compiling main.c (lib mode) ---"
gcc "${CFLAGS[@]}" -fPIC -DDRM_LIB_BUILD -c "$HERE/main.c" -o "$BUILD/main_lib.o"

# ── Link shared library ───────────────────────────────────────────────────────
echo "--- linking libdrm-native.so ---"
g++ -shared \
    "$BUILD/main_lib.o" \
    "$BUILD/cmdline.o" \
    "$BUILD/hybris_stubs.o" \
    "$BUILD/hybris_ctor.o" \
    "$BUILD/cjson.o" \
    "$BUILD/main_cpp.o" \
    "$BUILD/drm_lib.o" \
    "$DOBBY_BUILD/libdobby.a" \
    -L"$HYBRIS_BUILD" -lhybris-core \
    -lcurl \
    -lpthread \
    -ldl \
    -lm \
    -Wl,-rpath,"\$ORIGIN" \
    -o "$BUILD/libdrm-native.so"

echo ""
echo "=== Build successful ==="
echo "  Binary: $BUILD/drm-native"
echo "  Library: $BUILD/libdrm-native.so"
echo ""
echo "Run with:"
echo "  HYBRIS_LINKER_DIR=/tmp/hybris-linker \\"
echo "  HYBRIS_LD_LIBRARY_PATH=<rootfs>/system/lib64 \\"
echo "  HYBRIS_ANDROID_LIB64=<rootfs>/system/lib64 \\"
echo "  $BUILD/drm-native --base-dir <base_dir> ..."
