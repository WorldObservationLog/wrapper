# wrapper
A tool to decrypt Apple Music's music. An active subscription is still needed.

Only support Linux x86_64 and arm64.

# Install
Get the pre-built version from this project's Actions. 

Or you can refer to the Actions configuration file for compilation.

# Docker
Available for x86_64 and arm64. Need to download prebuilt version from releases or actions.

Build image: `docker build --tag wrapper .`

Login: `docker run -v ./rootfs/data:/app/rootfs/data -p 10020:10020 -e args="-L username:password -F -H 0.0.0.0" wrapper`

Run: `docker run -v ./rootfs/data:/app/rootfs/data -p 10020:10020 -e args="-H 0.0.0.0" wrapper`

# Usage
```
Usage: wrapper [OPTION]...

  -h, --help              Print help and exit
  -V, --version           Print version and exit
  -H, --host=STRING         (default=`127.0.0.1')
  -D, --decrypt-port=INT    (default=`10020')
  -M, --m3u8-port=INT       (default=`20020')
  -A, --access-port=INT     (default=`30020')
  -P, --proxy=STRING        (default=`')
  -L, --login=STRING        (username:password)
  -F, --code-from-file      (default=off)
```

# Build from source

1. Install dependencies:

- Build tools:

  ```
  sudo apt install build-essential cmake wget unzip git
  ```

- LLVM:

  ```
  sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)
  ```

- Android NDK r23b:
  ```
  wget -O android-ndk-r23b-linux.zip https://dl.google.com/android/repository/android-ndk-r23b-linux.zip
  unzip -q -d ~ android-ndk-r23b-linux.zip
  ```

2. Build

```
git clone https://github.com/glomatico/wrapper
cd wrapper
mkdir build
cd build
cmake ..
make -j$(nproc)
```


# Special thanks
- Anonymous, for providing the original version of this project and the legacy Frida decryption method.
- chocomint, for providing support for arm64 arch.
