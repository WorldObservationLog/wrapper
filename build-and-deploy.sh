#!/usr/bin/env bash
# build-and-deploy.sh — compile drm-native and deploy it into the apple-music-linux
# drm/ directory so the engine picks it up automatically at startup.
#
# Usage:
#   cd "/home/daksh/Git Projects/wrapper"
#   bash build-and-deploy.sh
#
# What it does:
#   1. Compiles drm-native via build-native.sh
#   2. Copies the binary + libhybris-core.so + q.so into:
#        <aml>/drm/drm-native
#        <aml>/drm/libhybris-core.so    (rpath dependency, co-located with binary)
#        <aml>/drm/hybris-linker/q.so   (HYBRIS_LINKER_DIR = drm/hybris-linker/)
#
# The engine (apiserver.go) already prefers drm-native over drm-rootless when
# drm/drm-native exists, and auto-sets HYBRIS_* env vars:
#   HYBRIS_LINKER_DIR      = <binaryDir>/hybris-linker
#   HYBRIS_LD_LIBRARY_PATH = <binaryDir>/rootfs/system/lib64
#   HYBRIS_ANDROID_LIB64  = <binaryDir>/rootfs/system/lib64
# so no engine changes are needed.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
AML_DRM="/home/daksh/Git Projects/apple-music-linux/drm"

NATIVE_BIN=/tmp/wrapper-native/drm-native
HYBRIS_LIB=/tmp/hybris-x86_64-build/libhybris-core.so
LINKER_SO=/tmp/hybris-linker/q.so

# ── 1. Compile ────────────────────────────────────────────────────────────────
echo "=== Building drm-native ==="
bash "$HERE/build-native.sh"

# ── 2. Verify build artefacts exist ──────────────────────────────────────────
for f in "$NATIVE_BIN" "$HYBRIS_LIB" "$LINKER_SO"; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: required file not found: $f"
        exit 1
    fi
done

# ── 3. Deploy ─────────────────────────────────────────────────────────────────
echo ""
echo "=== Deploying to $AML_DRM ==="

mkdir -p "$AML_DRM/hybris-linker"

cp -v "$NATIVE_BIN"  "$AML_DRM/drm-native"
cp -v "$HYBRIS_LIB"  "$AML_DRM/libhybris-core.so"
cp -v "$LINKER_SO"   "$AML_DRM/hybris-linker/q.so"

chmod +x "$AML_DRM/drm-native"

# ── 4. Patch rpath so the binary finds libhybris-core.so next to itself ───────
# build-native.sh hard-codes rpath=/tmp/hybris-x86_64-build which won't exist
# on another machine.  Patch it to $ORIGIN (= same directory as the binary).
if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN' "$AML_DRM/drm-native"
    echo "rpath patched to \$ORIGIN"
else
    echo "WARNING: patchelf not found — rpath still points to /tmp/hybris-x86_64-build"
    echo "         Install patchelf or the binary will only run on this machine."
fi

# ── 5. Also deploy into dist/ if it exists (running app picks it up) ──────────
DIST_DRM="/home/daksh/Git Projects/apple-music-linux/electron/dist/resources/drm"
if [[ -d "$DIST_DRM" ]]; then
    echo ""
    echo "=== Also deploying to dist/resources/drm ==="
    mkdir -p "$DIST_DRM/hybris-linker"
    cp -v "$AML_DRM/drm-native"             "$DIST_DRM/drm-native"
    cp -v "$AML_DRM/libhybris-core.so"      "$DIST_DRM/libhybris-core.so"
    cp -v "$AML_DRM/hybris-linker/q.so"     "$DIST_DRM/hybris-linker/q.so"
    chmod +x "$DIST_DRM/drm-native"
fi

echo ""
echo "=== Done ==="
echo "  drm-native:        $AML_DRM/drm-native"
echo "  libhybris-core.so: $AML_DRM/libhybris-core.so"
echo "  hybris-linker/q.so: $AML_DRM/hybris-linker/q.so"
echo ""
echo "Start the engine normally — it will auto-select drm-native over drm-rootless."
