# wrapper-lite QEMU

Run wrapper-lite inside `qemu-system-x86_64`.

## Layout

```text
wrapper-lite-qemu.cpp       # launcher source (repository root)
wrapper-lite-qemu           # launcher binary
qemu/
├── vmlinuz-lite-qemu
├── lite-initramfs.cpio.gz
├── data.img
├── build.sh
├── mkdata.sh
├── init
└── README.md
```

## Build QEMU assets

```bash
apt-get download busybox-static
./qemu/build.sh
```

## Build launcher

```bash
c++ -std=c++11 -O2 -o wrapper-lite-qemu wrapper-lite-qemu.cpp
```

## Run

```bash
./wrapper-lite-qemu --help
./wrapper-lite-qemu --login user:pass --base-dir /data
./wrapper-lite-qemu
```

All arguments except `--accel` are forwarded line-by-line to wrapper-lite inside the guest.

## Acceleration

- Linux: auto-detect KVM.
- macOS: try HVF, fall back to TCG.
- Windows: try WHPX, fall back to TCG.

Force acceleration:

```bash
./wrapper-lite-qemu --accel kvm ...
./wrapper-lite-qemu --accel hvf ...
./wrapper-lite-qemu --accel whpx ...
./wrapper-lite-qemu --accel tcg ...
```

Windows WHPX uses:

```text
-accel whpx,kernel-irqchip=off
-cpu qemu64-v1
```

## Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `HOST_PORT` | `8080` | host port |
| `GUEST_PORT` | `8080` | guest port |
| `MEMORY` | `512` | guest memory in MB |
| `SMP` | `2` | guest CPU count |
| `LITE_QEMU_ACCEL` | auto | force acceleration |
| `QEMU_BIN` | auto | QEMU binary path |

## QEMU lookup order

1. `QEMU_BIN`
2. `PATH`
3. `qemu/bin/`
