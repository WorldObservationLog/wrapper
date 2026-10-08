# syntax=docker/dockerfile:1
# Optimized for wrapper-lite (podman or docker)
ARG BUILD_PLATFORM=linux/amd64
ARG RUNTIME_PLATFORM=linux/amd64

# ---------- build stage ----------
FROM --platform=${BUILD_PLATFORM} debian:13.2 AS build
ARG NDK_VERSION=23
SHELL ["/bin/bash", "-c"]
WORKDIR /app

# apt cache mounts keep downloaded packages out of image layers
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake unzip git lsb-release gnupg aria2 wget ca-certificates

RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

# NDK is extracted to /app/android-ndk-r23b, which CMakeLists.txt expects
RUN aria2c -o android-ndk-r${NDK_VERSION}b-linux.zip \
        https://dl.google.com/android/repository/android-ndk-r${NDK_VERSION}b-linux.zip \
    && unzip -q -d /app android-ndk-r${NDK_VERSION}b-linux.zip \
    && rm android-ndk-r${NDK_VERSION}b-linux.zip

# Copy source after the slow toolchain layers so code edits stay cached
COPY ./ ./

# Build only what the runtime needs:
#  - lite: the Android-layer binary (rootfs/system/bin/lite)
#  - wrapper_lite_rootless_exe: produces /app/wrapper-lite-rootless
RUN mkdir -p build \
    && cmake -S /app -B /app/build -DCMAKE_BUILD_TYPE=Release -DBUILD_HOST_LAUNCHERS=ON \
    && cmake --build /app/build --target lite wrapper_lite_rootless_exe -j"$(nproc)"

# ---------- runtime stage ----------
FROM --platform=${RUNTIME_PLATFORM} debian:13.2
WORKDIR /app

COPY --from=build --chmod=755 /app/wrapper-lite-rootless /app/wrapper-lite-rootless
COPY --from=build /app/rootfs /app/rootfs
COPY --chmod=755 entrypoint.sh /app/entrypoint.sh

CMD ["/app/entrypoint.sh"]

# wrapper-lite listens on 12340 (entrypoint.sh and README)
EXPOSE 12340