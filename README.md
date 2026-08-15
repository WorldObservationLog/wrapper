# wrapper

A tool to decrypt Apple Music songs. An active subscription is still needed.

Supports only x86_64 and arm64 Linux.

## Installation

Installation methods:

- [Docker](#docker) (recommended)
- Prebuilt binaries (from [releases](https://github.com/WorldObservationLog/wrapper/releases) or [actions](https://github.com/WorldObservationLog/wrapper/actions))
- [Build from source](#build-from-source)

### Docker

Available for x86_64 and arm64. Need to download prebuilt version from releases or actions.

1. Build image:

```
docker build --tag ghcr.io/worldobservationlog/wrapper:local .
```

2. Login:

```
docker run --privileged --rm -it -v ./rootfs/data:/app/rootfs/data --entrypoint ./wrapper ghcr.io/worldobservationlog/wrapper:local -L "username:password" -H 0.0.0.0
```

Quit after this (using Ctrl-C).

3. Run:

```
docker run --privileged -v ./rootfs/data:/app/rootfs/data -p 10020:10020 -p 20020:20020 -p 30020:30020 -e args="-H 0.0.0.0" ghcr.io/worldobservationlog/wrapper:local
```


### Build from source

1. Install dependencies:

- Build tools:

  ```
  sudo apt install build-essential cmake curl unzip git
  ```

- LLVM:

  ```
  sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
  ```

- Android NDK r23b:
  ```
  curl -fLO https://dl.google.com/android/repository/android-ndk-r23b-linux.zip
  unzip -d . android-ndk-r23b-linux.zip
  ```

2. Build:

```
git clone https://github.com/WorldObservationLog/wrapper
cd wrapper
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Usage

```
Usage: wrapper [OPTION]...

  -h, --help              Print help and exit
  -V, --version           Print version and exit
  -H, --host=STRING         (default=`127.0.0.1')
  -D, --decrypt-port=INT    (default=`10020')
  -M, --m3u8-port=INT       (default=`20020')
  -A, --account-port=INT    (default=`30020')
  -K, --key-port=INT        (default=`40020')
  -P, --proxy=STRING        (default=`')
  -L, --login=STRING        (username:password)
  -F, --code-from-file      (default=off)
```

## Services (4 TCP ports)

| Port | Option | Protocol | Purpose |
|------|--------|----------|---------|
| 10020 | `-D` | Binary | Sample decryption: `[1B len][adamId][1B len][uri]` then loop `[4B size][ciphertext]` → plaintext |
| 20020 | `-M` | Binary | M3U8 stream URL: `[1B len][adamId digits]` → M3U8 URL |
| 30020 | `-A` | HTTP | Account info JSON |
| 40020 | `-K` | HTTP | Key service: `?adamId=&uri=` → `{contentKey, ctx, state, rcx/rax/rdx/r9/rbp}` decryption template |

### 40020 key service

Request any track once to get the complete content decryption template:

```bash
curl "http://127.0.0.1:40020/?adamId=1720704575&uri=skd%3A%2F%2Fitunes.apple.com%2Fp683167092%2Fc6"
# → {"adamId":..., "keyUri":..., "contentKey":..., "ctx":"<base64>", "state":"<base64>",
#    "rcx":"0x..","rax":"0x..","rdx":"0x..","r9":"0x..","rbp":"0x.."}
```

The template is captured by a Dobby hook at the R1 entry (`libCoreLSKD+0x1d5709`) in debug builds.

## Special thanks

- Anonymous, for providing the original version of this project and the legacy Frida decryption method.
- chocomint, for providing support for arm64 arch.
