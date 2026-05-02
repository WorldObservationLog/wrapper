#!/bin/sh
set -e

TOKEN_DB_PATH="/app/rootfs/data/data/com.apple.android.music/files/mpl_db/kvs.sqlitedb"

if [ ! -f "$TOKEN_DB_PATH" ]; then
  echo "Login required: account database not found."
  if [ -z "${USERNAME}" ] || [ -z "${PASSWORD}" ]; then
    echo "Error: USERNAME and PASSWORD environment variables must be set when account database is missing." >&2
    exit 1
  fi
  exec ./wrapper \
    -L ${USERNAME}:${PASSWORD} \
    -F \
    -H 0.0.0.0 \
    "$@"
else
  exec ./wrapper \
    -H 0.0.0.0 \
    "$@"
fi
