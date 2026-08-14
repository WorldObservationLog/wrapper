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

| Port | Option | Protocol | 用途 |
|------|--------|----------|------|
| 10020 | `-D` | 二进制 | 样本解密: `[1B len][adam][1B len][uri]` + 循环 `[4B size][密文]→明文` |
| 20020 | `-M` | 二进制 | M3U8 流地址: `[1B len][adamId数字]` → M3U8 URL |
| 30020 | `-A` | HTTP | 账号信息 JSON |
| 40020 | `-K` | HTTP | **key 服务**: `?adamId=&uri=` → `{contentKey, ctx, state, rcx/rax/rdx/r9/rbp}` 解密模板 |

### 40020 key 服务（离线解密模板）

对任意音轨请求一次, 返回完整的 content 解密模板, 供纯 Python 离线解密器使用:

```bash
curl "http://127.0.0.1:40020/?adamId=1720704575&uri=skd%3A%2F%2Fitunes.apple.com%2Fp683167092%2Fc6"
# → {"adamId":..., "keyUri":..., "contentKey":..., "ctx":"<base64>", "state":"<base64>",
#    "rcx":"0x..","rax":"0x..","rdx":"0x..","r9":"0x..","rbp":"0x.."}
```

模板由 R1 入口 (libCoreLSKD+0x1d5709) 的 Dobby hook 捕获 (debug 构建), 详见
`decryption/docs/offline-decryption.md`。

## Offline decryption（纯 Python 离线解密）

本仓库含一套完整的纯 Python 解密实现 (`decryption/`), 无需 Qiling / 原生库:

```bash
# 验证 wrapper 解密正确性 (专辑《戀曲2000》10 音轨 prefetch + content)
python decryption/tests/test_main_go.py          # → 20/20 PASS

# 最小化单曲解密: M3U8 → 逐样本解密 → 可播放 m4a
python decryption/src/decrypt_song.py <adamId> --variant gr256 --out song.m4a

# 直接解密 content 样本 (40020 实时获取模板)
python decryption/src/decrypt_tool.py <key> <密文> --content-server <adamId> <keyUri> 127.0.0.1:40020
```

详见 `decryption/README.md`。

## Security note

`rootfs/data/` 含真实 Apple Music 账户会话 / contentKey 等敏感数据 (已 gitignore,
不会提交)。请勿将 `rootfs/data` 或 `decryption/research/data/keys` 下内容公开。

## Special thanks

- Anonymous, for providing the original version of this project and the legacy Frida decryption method.
- chocomint, for providing support for arm64 arch.
