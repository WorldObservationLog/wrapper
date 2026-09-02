ARG BUILD_PLATFORM=linux/amd64
ARG RUNTIME_PLATFORM=linux/amd64

FROM --platform=${BUILD_PLATFORM} debian:13.2 AS build
ARG TARGET_ARCH=amd64
ARG NDK_VERSION=23

SHELL ["/bin/bash", "-c"]

WORKDIR /app

# Skip SDK download if a prebuilt wrapper binary is provided
COPY ./*wrapper ./
RUN if [[ -f ./wrapper ]]; then \
        touch /use_prebuild; \
    fi

RUN --mount=type=cache,target=/var/lib/apt,sharing=locked \
    --mount=type=cache,target=/var/cache/apt,sharing=locked \
    if [[ ! -f /use_prebuild ]]; then \
        apt-get update; \
        apt-get install -y \
            build-essential \
            cmake \
            unzip \
            git \
            lsb-release \
            gnupg \
            aria2; \
    fi

RUN if [[ ! -f /use_prebuild ]]; then \
      bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"; \
    fi

RUN if [[ ! -f /use_prebuild ]]; then \
        aria2c -o android-ndk-r${NDK_VERSION}b-linux.zip https://dl.google.com/android/repository/android-ndk-r${NDK_VERSION}b-linux.zip; \
        unzip -q -d /app android-ndk-r${NDK_VERSION}b-linux.zip; \
        rm android-ndk-r${NDK_VERSION}b-linux.zip; \
    fi

WORKDIR /app
COPY ./ ./
RUN if [[ ! -f /use_prebuild ]]; then \
        mkdir -p build; \
        cmake -S /app -B /app/build -DTARGET_ARCH=${TARGET_ARCH}; \
        cmake --build /app/build -j$(nproc); \
    elif [[ -f wrapper ]]; then \
        chmod +x /app/wrapper; \
    else \
        echo "ERROR: Neither CMakeLists.txt nor prebuilt /app/wrapper found in build context." >&2 && \
        ls -la /app >&2 && \
        exit 1; \
    fi

FROM --platform=${RUNTIME_PLATFORM} debian:13.2

WORKDIR /app
COPY --from=build /app/wrapper /app/wrapper
COPY --from=build /app/rootfs /app/rootfs
COPY entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

CMD ["/app/entrypoint.sh"]

EXPOSE 10020 20020 30020
