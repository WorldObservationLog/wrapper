ARG BUILD_PLATFORM=linux/amd64
ARG RUNTIME_PLATFORM=linux/amd64

FROM --platform=${BUILD_PLATFORM} debian:13.2 AS build
ARG NDK_VERSION=23

SHELL ["/bin/bash", "-c"]
WORKDIR /app

RUN apt-get update && apt-get install -y \
        build-essential cmake unzip git lsb-release gnupg aria2

RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

RUN aria2c -o android-ndk-r${NDK_VERSION}b-linux.zip https://dl.google.com/android/repository/android-ndk-r${NDK_VERSION}b-linux.zip \
    && unzip -q -d /app android-ndk-r${NDK_VERSION}b-linux.zip \
    && rm android-ndk-r${NDK_VERSION}b-linux.zip

COPY ./ ./

RUN mkdir -p build     && cmake -S /app -B /app/build -DCMAKE_BUILD_TYPE=Release -DBUILD_HOST_LAUNCHERS=ON -DLITE_INTERP=/app/lib/linker64     && cmake --build /app/build --target wrapper_lite_exe wrapper_lite_rootless_exe -j$(nproc)     && cmake --build /app/build -j$(nproc)

FROM --platform=${RUNTIME_PLATFORM} debian:13.2

WORKDIR /app
COPY --from=build /app/rootfs/system/bin/lite /app/lite
COPY --from=build /app/rootfs/system/bin/linker64 /app/lib/linker64
COPY --from=build /app/rootfs/system/lib64/ /app/lib/
COPY --from=build /app/rootfs/system/etc/security/cacert.pem /app/etc/cacert.pem
COPY --from=build /app/rootfs/system/usr/share/zoneinfo/tzdata /system/usr/share/zoneinfo/tzdata
COPY entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh /app/lite /app/lib/linker64 \
 && mkdir -p /data

# lite is a bionic binary whose ELF interpreter was patched at build time to
# /app/lib/linker64: it runs as a plain Linux program — no chroot, no mounts,
# no user namespaces and no /system symlinks are needed at runtime.
ENV LD_LIBRARY_PATH=/app/lib \
    LITE_CA_BUNDLE=/app/etc/cacert.pem \
    ANDROID_ROOT=/system

CMD ["/app/entrypoint.sh"]

EXPOSE 12340
