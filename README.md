# wrapper

A high-performance daemon and native library to decrypt Apple Music streams on Linux. An active subscription is required.

Supports **x86_64** and **arm64** Linux.

---

## Architecture & Modes

`wrapper` supports three execution modes depending on your deployment environment:

| Mode | Binary / Target | Isolation | Description |
|---|---|---|---|
| **Host-Native (libhybris)** | `drm-native`<br>`libdrm-native.so` | None (In-Process) | **Recommended.** Loads Android Bionic `.so` libraries directly into a native glibc Linux process via libhybris. Eliminates container/proot overhead and enables in-process CGO linking. |
| **Rootless Container** | `wrapper-rootless` | User namespaces / proot | Runs unprivileged in userspace without requiring Docker or root permissions. |
| **Docker Container** | `wrapper` | Privileged container | Containerized deployment using Docker. |

### Daemon Resilience & Auto-Recovery
The daemon implements a recoverable state machine (`Running`, `Scheduled`, `Refreshing`, `Failed`):
- **Non-blocking Lease Callbacks:** Lease expiry (`endLeaseCb`) and playback error (`pbErrCb`) events are queued without blocking the library thread.
- **Dedicated Recovery Worker:** Automatically coalesces lease refreshes with exponential backoff (1s → 2s → 5s → 10s → 30s).
- **Request Gating & Thread Safety:** Mutex-protected FairPlay context reads; HTTP requests gracefully gate during re-authentication rather than terminating the process with `exit(1)`.

---

## Installation & Building

### 1. Host-Native Build via Libhybris (Fastest, No NDK Required)

Compiles `drm-native` and `libdrm-native.so` directly against glibc using host `gcc`/`g++` and libhybris.

#### Prerequisites
- Host build tools: `gcc`, `g++`, `curl`, `patchelf`
- Built `libhybris-core.so` and linker plugin `q.so`
- libhybris headers: set `HYBRIS_INC` to the `hybris/include` directory
- Dobby (tested at commit `e9fe7fb`): `dobby.h` and a built `libdobby.a` (set `DOBBY_SRC` / `DOBBY_BUILD`; defaults `/tmp/dobby-src`, `/tmp/dobby-build`)
  - On newer GCC, configure with `-DCMAKE_C_FLAGS="-include sys/time.h"`.
  - Dobby's `external/logging/logging/logging.h` needs `inline` on the `Logger::Shared()` definition, or linking fails with multiple definitions.
- Optional overrides: `HYBRIS_LIB` / `LINKER_SO` (paths to `libhybris-core.so` and `q.so`), `DEPLOY_DIR_EXTRA`.

#### Build
```bash
# One-shot build (outputs to /tmp/wrapper-native)
bash build-native.sh

# Or build and deploy to a target directory:
DEPLOY_DIR=/path/to/drm bash build-and-deploy.sh
```

#### Runtime Environment
When executing `drm-native` directly, configure the hybris environment paths:
```bash
export HYBRIS_LINKER_DIR=/path/to/hybris-linker
export HYBRIS_LD_LIBRARY_PATH=/path/to/rootfs/system/lib64
export HYBRIS_ANDROID_LIB64=/path/to/rootfs/system/lib64

./drm-native --base-dir /path/to/data/files
```

---

### 2. In-Process C / CGO Library (`drm_lib`)

When compiled with `build-native.sh`, `libdrm-native.so` exposes a C API defined in [drm_lib.h](drm_lib.h) that can be embedded directly into Go (via CGO) or C/C++ applications without socket IPC:

```c
#include "drm_lib.h"

drm_lib_config_t cfg = {
    .base_dir  = "/path/to/data",
    .lib64_dir = "/path/to/rootfs/system/lib64",
    .auth_cb   = my_auth_callback,
    .state_cb  = my_state_callback,
};

if (drm_lib_init(&cfg) == 0) {
    /* decrypt sample in-process */
    drm_lib_decrypt(kd_ctx, sample_buffer, sample_size);
    drm_lib_shutdown();
}
```

---

### 3. Docker

Available for x86_64 and arm64.

1. **Build image:**
   ```bash
   docker build --tag ghcr.io/worldobservationlog/wrapper:local .
   ```

2. **Initial Login:**
   ```bash
   docker run --privileged --rm -it \
     -v ./rootfs/data:/app/rootfs/data \
     -entrypoint ./wrapper ghcr.io/worldobservationlog/wrapper:local \
     -L "username:password" -H 0.0.0.0
   ```
   *(Exit using Ctrl-C after authentication succeeds).*

3. **Run Daemon:**
   ```bash
   docker run --privileged \
     -v ./rootfs/data:/app/rootfs/data \
     -p 10020:10020 -p 20020:20020 -p 30020:30020 -p 40020:40020 -p 50020:50020 -p 60020:60020 \
     -e args="-H 0.0.0.0" \
     ghcr.io/worldobservationlog/wrapper:local
   ```

---

### 4. Build from Source via Android NDK (Legacy / Rootless)

Builds the Bionic-linked `main` executable, `wrapper`, and `wrapper-rootless`.

1. **Install dependencies:**
   ```bash
   sudo apt install build-essential cmake curl unzip git
   sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
   ```

2. **Download Android NDK r23b:**
   ```bash
   curl -fLO https://dl.google.com/android/repository/android-ndk-r23b-linux.zip
   unzip -d . android-ndk-r23b-linux.zip
   ```

3. **Build:**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

---

## Usage & CLI Options

```text
Usage: wrapper [OPTION]...

  -h, --help                Print help and exit
  -V, --version             Print version and exit
  -H, --host=STRING         Host to bind on (default: 127.0.0.1)
  -D, --decrypt-port=INT    Decryption server port (default: 10020)
  -M, --m3u8-port=INT       M3U8 / playlist proxy port (default: 20020)
  -A, --account-port=INT    Account management port (default: 30020)
  -K, --key-port=INT        Key service port (default: 40020)
  -G, --mv-port=INT         Music video port (default: 50020)
  -P, --proxy=STRING        HTTP proxy URL (default: none)
  -L, --login=STRING        Apple ID login credentials (username:password)
  -F, --code-from-file      Read 2FA code from file rather than stdin (default: off)
  -B, --base-dir=STRING     Base data directory (default: /data/data/com.apple.android.music/files)
  -I, --device-info=STRING  9-field client device descriptor
```

## Services (6 TCP ports)

| Port | Option | Protocol | Purpose |
|------|--------|----------|---------|
| 10020 | `-D` | Binary | Sample decryption: `[1B len][adamId][1B len][uri]` then loop `[4B size][ciphertext]` → plaintext |
| 20020 | `-M` | Binary | M3U8 stream URL: `[1B len][adamId digits]` → M3U8 URL |
| 30020 | `-A` | HTTP | Account info JSON |
| 40020 | `-K` | HTTP | Key service: `?adamId=&uri=` → `{contentKey, ctx, state, rcx/rax/rdx/r9/rbp}` decryption template |
| 50020 | `-G` | see source | Progressive music-video (MV) service (`new_socket_mv`) |
| 60020 | `-G` + 10000 | see source | itun FairPlay decrypt for progressive MV (`new_socket_itun`) |

### 40020 key service

Request any track once to get the complete content decryption template:

```bash
curl "http://127.0.0.1:40020/?adamId=1720704575&uri=skd%3A%2F%2Fitunes.apple.com%2Fp683167092%2Fc6"
# → {"adamId":..., "keyUri":..., "contentKey":..., "ctx":"<base64>", "state":"<base64>",
#    "rcx":"0x..","rax":"0x..","rdx":"0x..","r9":"0x..","rbp":"0x.."}
```

The template is captured by a Dobby hook at the R1 entry (`libCoreLSKD+0x1d5709`) in debug builds.

---

## Development & Testing

- **Wrapper Drift Check:** Verify synchronization between privileged and rootless wrapper code:
  ```bash
  python3 scripts/check-drift.py
  ```

---

## Special thanks

- Anonymous, for providing the original version of this project and the legacy Frida decryption method.
- chocomint, for providing support for arm64 arch.
