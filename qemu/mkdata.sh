#!/usr/bin/env bash
set -euo pipefail
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_DIR="${REPO_DIR}/qemu"
DATA_IMG="${QEMU_DIR}/data.img"
ROOTFS_DATA="${REPO_DIR}/rootfs/data"

if ! command -v mke2fs >/dev/null 2>&1; then
    echo "[mkdata] mke2fs not found (install e2fsprogs)" >&2
    exit 1
fi

echo "[mkdata] creating 64MB ext4 data disk from ${ROOTFS_DATA}"
truncate -s 64M "$DATA_IMG"
mke2fs -q -t ext4 -d "$ROOTFS_DATA" -F "$DATA_IMG"
echo "[mkdata] done: ${DATA_IMG}"
