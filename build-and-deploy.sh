#!/usr/bin/env bash
# build-and-deploy.sh: compile drm-native and copy it, with its runtime
# dependencies, into a target directory.
#
# Usage:
#   DEPLOY_DIR=/path/to/drm bash build-and-deploy.sh
#
# Optional environment variables:
#   DEPLOY_DIR_EXTRA  second directory to receive the same files
#   NATIVE_BIN, NATIVE_SO, HYBRIS_LIB, LINKER_SO  override artefact paths
#
# Layout produced in DEPLOY_DIR:
#   drm-native
#   libdrm-native.so
#   libhybris-core.so        (rpath dependency, co-located with the binary)
#   hybris-linker/q.so       (use as HYBRIS_LINKER_DIR)

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

: "${DEPLOY_DIR:?Set DEPLOY_DIR to the target directory}"
NATIVE_BIN="${NATIVE_BIN:-/tmp/wrapper-native/drm-native}"
NATIVE_SO="${NATIVE_SO:-/tmp/wrapper-native/libdrm-native.so}"
HYBRIS_LIB="${HYBRIS_LIB:-/tmp/hybris-x86_64-build/libhybris-core.so}"
LINKER_SO="${LINKER_SO:-/tmp/hybris-linker/q.so}"

echo "=== Building drm-native ==="
bash "$HERE/build-native.sh"

for f in "$NATIVE_BIN" "$NATIVE_SO" "$HYBRIS_LIB" "$LINKER_SO"; do
  [[ -f "$f" ]] || { echo "ERROR: required file not found: $f"; exit 1; }
done

deploy() {
  local dir="$1"
  echo; echo "=== Deploying to $dir ==="
  mkdir -p "$dir/hybris-linker"
  cp -v "$NATIVE_BIN"  "$dir/drm-native"
  cp -v "$NATIVE_SO"   "$dir/libdrm-native.so"
  cp -v "$HYBRIS_LIB"  "$dir/libhybris-core.so"
  cp -v "$LINKER_SO"   "$dir/hybris-linker/q.so"
  chmod +x "$dir/drm-native"
  # build-native.sh hard-codes an rpath under /tmp; make it relative.
  if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN' "$dir/drm-native"
    echo "rpath patched to \$ORIGIN"
  else
    echo "WARNING: patchelf not found; binary will only run on this machine"
  fi
}

deploy "$DEPLOY_DIR"
[[ -n "${DEPLOY_DIR_EXTRA:-}" ]] && deploy "$DEPLOY_DIR_EXTRA"

echo; echo "=== Done ==="
echo "Runtime env for drm-native:"
echo "  HYBRIS_LINKER_DIR=$DEPLOY_DIR/hybris-linker"
echo "  HYBRIS_LD_LIBRARY_PATH=<rootfs>/system/lib64"
echo "  HYBRIS_ANDROID_LIB64=<rootfs>/system/lib64"
