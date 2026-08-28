# wrapper-lite 1.0.0

A lightweight single-port HTTP wrapper for Apple Music decryption.

It provides five endpoints on one HTTP port: M3U8, Key, Lyrics, License, and WebPlayback.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Debug build:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

Build outputs:

- `rootfs/system/bin/lite` — Android-layer binary
- `wrapper-lite` — privileged host launcher
- `wrapper-lite-rootless` — rootless host launcher (user namespaces)

## Run natively

```bash
# Login, cache tokens, then exit
./wrapper-lite-rootless --login user:pass --code-from-file --base-dir /data

# Start the HTTP service
./wrapper-lite-rootless --base-dir /data --host 0.0.0.0 --port 8080
```

2FA code:

- Interactive prompt when a TTY is available.
- With `--code-from-file`, the code is read from `data/2fa.txt`.

## Run with QEMU

```bash
c++ -std=c++11 -O2 -o wrapper-lite-qemu wrapper-lite-qemu.cpp
./wrapper-lite-qemu --help
./wrapper-lite-qemu
```

QEMU assets live in `qemu/`. The launcher searches for QEMU in:

1. `QEMU_BIN`
2. `PATH`
3. `qemu/bin/`

## Docker

```bash
docker build -t wrapper-lite:local .
docker run --privileged -p 8080:8080 \
  -v ./rootfs/data:/app/rootfs/data \
  -e USERNAME=... -e PASSWORD=... \
  wrapper-lite:local
```

## HTTP API

All responses use:

```json
{"code":0,"msg":"SUCCESS","data":{...}}
```

| Endpoint | Method | Parameters |
|----------|--------|------------|
| `/m3u8` | GET | `adamId` |
| `/key` | GET | `adamId`, optional `uri` |
| `/lyrics` | GET | `adamId`, optional `language`, optional `syllable` (`1`=syllable-lyrics default, `0`=lyrics) |
| `/webplayback` | GET | `adamId` |
| `/license` | POST | JSON: `adamId`, `challenge`, `uri` |
| `/status` | GET | returns `storefront` |

## lite arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--login user:pass` | — | login, cache tokens, then exit |
| `--code-from-file` | off | read 2FA code from file |
| `--host` | `127.0.0.1` | listen address |
| `--port` | `8080` | listen port |
| `--base-dir` | `data` | data directory |
| `--device-info` | built-in | device info string |
| `--proxy` | — | proxy URL |
| `--debug` | off | debug logging; disables SSL verify in Release |
| `--log-level` | `info` | `debug`, `info`, `warn`, `error` |
| `--log-file` | — | log file path |
| `--token-refresh-interval` | `1800` | background refresh interval in seconds |

## QEMU launcher arguments

| Argument | Description |
|----------|-------------|
| `--accel kvm` | force KVM (Linux) |
| `--accel hvf` | force HVF (macOS) |
| `--accel whpx` | force WHPX (Windows) |
| `--accel tcg` | force TCG |

Environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `HOST_PORT` | `8080` | host port |
| `GUEST_PORT` | `8080` | guest port |
| `MEMORY` | `512` | guest memory in MB |
| `SMP` | `2` | guest CPU count |
| `LITE_QEMU_ACCEL` | auto | force acceleration |
| `QEMU_BIN` | auto | QEMU binary path |

## Build notes

- Release: SSL verification is enabled by default. Pass `--debug` to disable it temporarily.
- Debug: SSL verification is disabled by default.

## License

This project is licensed under the [MIT License](LICENSE).
