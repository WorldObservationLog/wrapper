#!/usr/bin/env bash
set -euo pipefail
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOTFS_DIR="${REPO_DIR}/rootfs"
BUSYBOX_DEB="$(ls -1 "${REPO_DIR}"/busybox-static_*_amd64.deb 2>/dev/null | head -1 || true)"
if [[ -z "$BUSYBOX_DEB" && "$(command -v apt-get || true)" != "" ]]; then
    echo "[build] busybox-static deb not found, downloading..."
    (cd "$REPO_DIR" && apt-get download busybox-static >/dev/null 2>&1 || true)
    BUSYBOX_DEB="$(ls -1 "${REPO_DIR}"/busybox-static_*_amd64.deb 2>/dev/null | head -1 || true)"
fi
OUT_DIR="${REPO_DIR}/qemu"
OUT_FILE="${OUT_DIR}/lite-initramfs.cpio.gz"
KERNEL_FILE="${OUT_DIR}/vmlinuz-lite-qemu"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

echo "[build] locating kernel..."
if [[ -n "${KERNEL_SRC:-}" && -f "$KERNEL_SRC" ]]; then
    KERNEL_SRC_PATH="$KERNEL_SRC"
else
    KERNEL_SRC_PATH="$(find /boot -maxdepth 1 -name 'vmlinuz-*' -readable 2>/dev/null | sort -V | tail -1 || true)"
    if [[ -z "$KERNEL_SRC_PATH" ]]; then
        KERNEL_SRC_PATH="$(ls -1 /boot/vmlinuz-* 2>/dev/null | sort -V | tail -1 || true)"
    fi
fi
if [[ -z "$KERNEL_SRC_PATH" || ! -f "$KERNEL_SRC_PATH" ]]; then
    echo "[build] no kernel found; set KERNEL_SRC=/path/to/vmlinuz" >&2
    exit 1
fi
KERNEL_VER="$(basename "$KERNEL_SRC_PATH" | sed 's/^vmlinuz-//')"
echo "[build] kernel: ${KERNEL_SRC_PATH} (${KERNEL_VER})"

if [[ ! -d "/lib/modules/${KERNEL_VER}/kernel" ]]; then
    echo "[build] modules for ${KERNEL_VER} missing, trying to install..."
    sudo apt-get update || true
    sudo apt-get install -y "linux-modules-${KERNEL_VER}" || true
fi

echo "[build] extracting busybox..."
dpkg-deb -x "$BUSYBOX_DEB" "$STAGE"

echo "[build] copying rootfs (this may take a moment)..."
(cd "$ROOTFS_DIR" && tar --exclude=./dev --exclude=./proc --exclude=./data -cf - .) | (cd "$STAGE" && tar -xf -)

echo "[build] installing init..."
install -m 0755 "${OUT_DIR}/init" "$STAGE/init"

echo "[build] copying kernel modules..."
E1000_KO="/lib/modules/${KERNEL_VER}/kernel/drivers/net/ethernet/intel/e1000/e1000.ko"
if [[ -f "$E1000_KO" ]]; then
    cp -f "$E1000_KO" "$STAGE/e1000.ko"
else
    echo "[build] WARNING: ${E1000_KO} not found; guest networking may not work" >&2
fi

for mod in qemu_fw_cfg virtio virtio_ring virtio_pci_modern_dev virtio_pci_legacy_dev virtio_pci virtio_blk crc16 crc32c_generic jbd2 mbcache ext4; do
    MOD_SRC="$(find "/lib/modules/${KERNEL_VER}/kernel" -name "${mod}.ko" 2>/dev/null | head -1 || true)"
    if [[ -n "$MOD_SRC" && -f "$MOD_SRC" ]]; then
        cp -f "$MOD_SRC" "$STAGE/${mod}.ko"
    else
        echo "[build] WARNING: ${mod}.ko not found" >&2
    fi
done

echo "[build] packaging initramfs..."
INITRAMFS_COMPRESSOR="${INITRAMFS_COMPRESSOR:-gzip}"
if [[ "$INITRAMFS_COMPRESSOR" == "lz4" ]] && python3 -c 'import lz4.frame' >/dev/null 2>&1; then
    (cd "$STAGE" && find . | cpio -o -H newc --quiet | python3 -c 'import sys,lz4.frame; sys.stdout.buffer.write(lz4.frame.compress(sys.stdin.buffer.read()))' > "$OUT_FILE")
elif [[ "$INITRAMFS_COMPRESSOR" == "zstd" ]] && command -v zstd >/dev/null 2>&1; then
    (cd "$STAGE" && find . | cpio -o -H newc --quiet | zstd -3 -q > "$OUT_FILE")
else
    (cd "$STAGE" && find . | cpio -o -H newc --quiet | gzip -9 > "$OUT_FILE")
fi

if [[ -r "$KERNEL_SRC_PATH" ]]; then
    cp -f "$KERNEL_SRC_PATH" "$KERNEL_FILE"
else
    echo "[build] kernel not readable, copying with sudo..."
    sudo cp -f "$KERNEL_SRC_PATH" "$KERNEL_FILE"
fi

if [[ ! -f "${OUT_DIR}/data.img" ]]; then
    echo "[build] data disk missing, creating..."
    "${OUT_DIR}/mkdata.sh"
fi

echo "[build] done:"
ls -lh "$OUT_FILE" "$KERNEL_FILE" "${OUT_DIR}/data.img"
