# wrapper-lite

A lightweight single-port HTTP wrapper for Apple Music decryption.

It provides five endpoints on one HTTP port: M3U8, Key, Lyrics, License, and WebPlayback.

> **Recommended way to run**: use the `wrapper-lite-qemu` launcher — it boots a
> self-contained QEMU guest and forwards the HTTP API to the host, so you don't
> need a rooted device or a native chroot. See
> [Run with QEMU](#run-with-qemu-recommended).

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
./wrapper-lite-rootless --base-dir /data --host 0.0.0.0 --port 12340
```

2FA code:

- Interactive prompt when a TTY is available.
- With `--code-from-file`, the code is read from `data/2fa.txt`.

## Run with QEMU (recommended)

`wrapper-lite-qemu` is the recommended launcher. It boots the prebuilt guest
(the `qemu/` assets) in QEMU and forwards the guest's HTTP service to the host,
so no rooted device or native chroot is required. Prebuilt releases already
bundle QEMU plus its firmware in `qemu/bin/` for Linux, macOS, Windows and
Android.

```bash
# one-time: build the launcher (or download a prebuilt release)
c++ -std=c++11 -O2 -o wrapper-lite-qemu wrapper-lite-qemu.cpp

# login first (forwards --login to the guest lite), then serve
./wrapper-lite-qemu --login user:pass --code-from-file
./wrapper-lite-qemu
```

The launcher locates the QEMU binary in this order:

1. `--qemu-bin <path>` / `QEMU_BIN`
2. `PATH`
3. `qemu/bin/` (bundled)

See [QEMU launcher arguments](#qemu-launcher-arguments) for the full option
list.

## Docker

```bash
docker build -t wrapper-lite:local .
docker run --privileged -p 12340:12340 \
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
| `/key` | GET | `adamId`, `uri` (required; the prefetch `skd://itunes.apple.com/P000000000/s1/e1` is rejected unless `adamId=0`) |
| `/lyrics` | GET | `adamId`, optional `language`, optional `syllable` (`1`=syllable-lyrics default, `0`=lyrics), optional `script` (transliteration script, default `en-Latn`) |
| `/webplayback` | GET | `adamId` |
| `/license` | POST | JSON: `adamId`, `challenge`, `uri` |
| `/status` | GET | returns `regions` (list of storefront codes this wrapper can serve) |

## lite arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--login user:pass` | — | login, cache tokens, then exit |
| `--code-from-file` | off | read 2FA code from file |
| `--host` | `127.0.0.1` | listen address |
| `--port` | `12340` | listen port |
| `--base-dir` | `data` | data directory |
| `--device-info` | auto | override the entire device-info string (see [Device info](#device-info)) |
| `--proxy` | — | proxy URL |
| `--debug` | off | debug logging; disables SSL verify in Release |
| `--log-level` | `info` | `debug`, `info`, `warn`, `error` |
| `--log-file` | — | log file path |
| `--token-refresh-interval` | `1800` | background refresh interval in seconds |

## Device info

`lite` identifies itself to Apple's servers with a device-info string of the
form:

```
ClientIdentifier/VersionIdentifier/PlatformIdentifier/ProductVersion/DeviceModel/BuildVersion/LocaleIdentifier/LanguageIdentifier/AndroidID
```

Example: `Music/5.0.2/Android/10/Pixel 8/7663314/en-US/en-US/<android-id>`.

The trailing `AndroidID` is derived automatically:

- During `--login`, it is the FNV-1a 64-bit hash of the account username and is
  persisted to `<base-dir>/ANDROID_ID`, so each account keeps a stable device
  identity across runs.
- In service mode (no username), the persisted `ANDROID_ID` is reused; if none
  exists, a built-in default is used.

Pass `--device-info <string>` to override the entire string explicitly.

## QEMU launcher arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--host <addr>` | `127.0.0.1` | host address the forwarded port binds to (`0.0.0.0` to expose) |
| `--host-port <port>` | `12340` | host port |
| `--guest-port <port>` | `12340` | guest port |
| `--guest-host <addr>` | `0.0.0.0` | address the guest lite listens on (must be reachable by QEMU forwarding) |
| `--memory <MB>` | `512` | guest memory in MB |
| `--smp <N>` | `2` | guest CPU count |
| `--accel <accel>` | auto | force acceleration (`kvm`, `hvf`, `whpx`, `tcg`) |
| `--qemu-bin <path>` | auto | QEMU binary path |

Environment variables (fallbacks; command-line flags take precedence):

| Variable | Default | Description |
|----------|---------|-------------|
| `LITE_QEMU_HOST` | `127.0.0.1` | host address the forwarded port binds to |
| `HOST_PORT` | `12340` | host port |
| `GUEST_PORT` | `12340` | guest port |
| `LITE_GUEST_HOST` | `0.0.0.0` | address the guest lite listens on |
| `MEMORY` | `512` | guest memory in MB |
| `SMP` | `2` | guest CPU count |
| `LITE_QEMU_ACCEL` | auto | force acceleration |
| `QEMU_BIN` | auto | QEMU binary path |

## Build notes

- Release: SSL verification is enabled by default. Pass `--debug` to disable it temporarily.
- Debug: SSL verification is disabled by default.

## License

This project is licensed under the [MIT License](LICENSE).
