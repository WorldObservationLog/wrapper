cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(ANDROID_NDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/android-ndk-r23b" CACHE PATH "NDK Path")
set(TOOLCHAIN "${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64")

set(CMAKE_C_COMPILER "${TOOLCHAIN}/bin/x86_64-linux-android22-clang")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN}/bin/x86_64-linux-android22-clang++")
set(C_COMPILER "${TOOLCHAIN}/bin/clang")

project(wrapper-lite VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.19
)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(ENABLE_TARGET_EXPORT OFF CACHE BOOL "" FORCE)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cjson)

FetchContent_Declare(
  dobby
  GIT_REPOSITORY https://github.com/BepInEx/Dobby.git
  GIT_TAG        master
)
set(DOBBY_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(dobby)

set(WRAPPER_SOURCE wrapper-lite.c)
set(WRAPPER_ROOTLESS_SOURCE wrapper-lite-rootless.c)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -O2")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-s")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-s")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -g -O0")
endif()

set(SYSTEM_LIB_DIR "${CMAKE_SOURCE_DIR}/rootfs/system/lib64")
find_library(ANDROIDAPPMUSIC_LIB androidappmusic PATHS ${SYSTEM_LIB_DIR})
find_library(STORESERVICESCORE_LIB storeservicescore PATHS ${SYSTEM_LIB_DIR})
find_library(MEDIAPLATFORM_LIB mediaplatform PATHS ${SYSTEM_LIB_DIR})
find_library(CXX_SHARED_LIB c++_shared PATHS ${SYSTEM_LIB_DIR})
find_library(CURL_SHARED_LIB curl PATHS ${SYSTEM_LIB_DIR})
find_library(LOG_SHARED_LIB log PATHS ${SYSTEM_LIB_DIR})

# ---- wrapper-lite: single-port HTTP service ----
set(LITE_SOURCE
    lite/lite_main.cpp
    lite/bridge.cpp
    lite/auth.cpp
    lite/playback.cpp
    lite/tokens.cpp
    lite/apple_api.cpp
)

add_executable(lite ${LITE_SOURCE})

target_include_directories(lite PRIVATE
    ${cjson_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lite
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${dobby_SOURCE_DIR}/include
)

target_link_libraries(lite PRIVATE
    cjson
    dobby
    ${CXX_SHARED_LIB}
    ${ANDROIDAPPMUSIC_LIB}
    ${STORESERVICESCORE_LIB}
    ${MEDIAPLATFORM_LIB}
    ${LOG_SHARED_LIB}
    ${CURL_SHARED_LIB}
    dl
)

set_target_properties(lite PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/rootfs/system/bin"
)

target_compile_options(lite PRIVATE -Wno-unused-variable)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(lite PRIVATE LITE_RELEASE)
endif()
target_link_options(lite PRIVATE -Wl,--unresolved-symbols=ignore-in-object-files)

# Host launchers are kept for native chroot/namespace use, but QEMU (run.sh)
# is the default startup path.  Build them only when explicitly requested.
option(BUILD_HOST_LAUNCHERS "Build wrapper-lite and wrapper-lite-rootless host launchers" ON)
if(BUILD_HOST_LAUNCHERS)
    ExternalProject_Add(
        wrapper_lite_exe
        PREFIX ${CMAKE_BINARY_DIR}/wrapper_build
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${C_COMPILER} -O3 -Wall -s -o wrapper-lite ${WRAPPER_SOURCE}
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        DEPENDS lite
    )

    ExternalProject_Add(
        wrapper_lite_rootless_exe
        PREFIX ${CMAKE_BINARY_DIR}/wrapper_build
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${C_COMPILER} -O3 -Wall -s -o wrapper-lite-rootless ${WRAPPER_ROOTLESS_SOURCE}
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        DEPENDS lite
    )
endif()
