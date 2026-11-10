#!/bin/sh
set -e

# The Android layer keeps its databases in <base-dir>/mpl_db (the dir the
# chroot launchers always created); kvs.sqlitedb there is the login-state
# marker.
TOKEN_DB_PATH="/data/mpl_db/kvs.sqlitedb"

mkdir -p /data/mpl_db

# Normalise volume ownership: older volumes or bind mounts from a host user
# can carry non-root uids, which the Android layer cannot use.
if [ "$(stat -c %U /data)" != "root" ] || [ "$(stat -c %G /data)" != "root" ]; then
  chown -R root:root /data
fi

# lite runs directly as a plain Linux program: its ELF interpreter was patched
# to /app/lib/linker64 at build time, and the bundled Android libraries are
# found through LD_LIBRARY_PATH=/app/lib.  No chroot, no mounts, no user
# namespaces and no /system symlinks are needed, so every container runtime
# (docker rootful/rootless, podman, WSL containers) works with default flags.

if [ ! -f "$TOKEN_DB_PATH" ]; then
  echo "Login required: account database not found."
  if [ -z "${USERNAME}" ] || [ -z "${PASSWORD}" ]; then
    echo "Error: USERNAME and PASSWORD environment variables must be set when account database is missing." >&2
    exit 1
  fi
  echo "Running login (place the 2FA code into /data/2fa.txt on the volume if required)..."
  /app/lite \
    --login "${USERNAME}:${PASSWORD}" \
    --code-from-file \
    --base-dir /data \
    "$@"
fi

exec /app/lite \
  --base-dir /data \
  --host 0.0.0.0 \
  --port 12340 \
  "$@"
