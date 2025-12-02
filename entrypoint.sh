#!/bin/sh
set -e

MUSIC_TOKEN_PATH="/app/rootfs/data/data/com.apple.android.music/files/MUSIC_TOKEN"

if [ ! -f "$MUSIC_TOKEN_PATH" ]; then
  echo "Login required: MUSIC_TOKEN not found."
  if [ -z "${USERNAME}" ] || [ -z "${PASSWORD}" ]; then
    echo "Error: USERNAME and PASSWORD environment variables must be set when MUSIC_TOKEN is missing." >&2
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
