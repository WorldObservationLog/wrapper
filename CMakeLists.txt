cmake_minimum_required(VERSION 3.25)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(ANDROID_NDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/android-ndk-r23b" CACHE PATH "NDK Path")
set(TOOLCHAIN "${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64")

set(CMAKE_C_COMPILER "${TOOLCHAIN}/bin/aarch64-linux-android22-clang")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN}/bin/aarch64-linux-android22-clang++")
set(C_COMPILER "${TOOLCHAIN}/bin/clang")

project(wrapper LANGUAGES C CXX)

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

set(CMDLINE_SOURCE cmdline.c)
set(HANDLE_SOURCE main.cpp)
set(MAIN_SOURCE main.c)
set(WRAPPER_SOURCE wrapper.c)
set(WRAPPER_ROOTLESS_SOURCE wrapper-rootless.c)
set(WRAPPER_ANDROID_SOURCE wrapper-android.c)

add_library(cmdline_object OBJECT ${CMDLINE_SOURCE})
add_library(handle_object OBJECT ${HANDLE_SOURCE})

add_executable(main ${MAIN_SOURCE} $<TARGET_OBJECTS:cmdline_object> $<TARGET_OBJECTS:handle_object>)

target_include_directories(main PRIVATE ${cjson_SOURCE_DIR})
target_link_libraries(main PRIVATE cjson)
target_link_libraries(main PRIVATE dobby)

set_target_properties(main PROPERTIES 
    # COMPILE_DEFINITIONS "MyRelease"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/rootfs/system/bin"
)

set(SYSTEM_LIB_DIR "${CMAKE_SOURCE_DIR}/rootfs/system/lib64")
find_library(ANDROIDAPPMUSIC_LIB androidappmusic PATHS ${SYSTEM_LIB_DIR})
find_library(STORESERVICESCORE_LIB storeservicescore PATHS ${SYSTEM_LIB_DIR})
find_library(MEDIAPLATFORM_LIB mediaplatform PATHS ${SYSTEM_LIB_DIR})
find_library(CXX_SHARED_LIB c++_shared PATHS ${SYSTEM_LIB_DIR})
find_library(CURL_SHARED_LIB curl PATHS ${SYSTEM_LIB_DIR})
find_library(LOG_SHARED_LIB log PATHS ${SYSTEM_LIB_DIR})

target_link_libraries(main PRIVATE
    ${CXX_SHARED_LIB}
    ${ANDROIDAPPMUSIC_LIB}
    ${STORESERVICESCORE_LIB}
    ${MEDIAPLATFORM_LIB}
)

get_target_property(MAIN_COMPILE_DEFINITIONS main COMPILE_DEFINITIONS)
if(NOT "MyRelease" IN_LIST MAIN_COMPILE_DEFINITIONS)
    target_link_libraries(main PRIVATE ${LOG_SHARED_LIB} ${CURL_SHARED_LIB})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -g")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
endif()

ExternalProject_Add(
    wrapper_exe
    PREFIX ${CMAKE_BINARY_DIR}/wrapper_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND aarch64-linux-gnu-gcc -O3 -Wall -o wrapper ${WRAPPER_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    DEPENDS main
)

ExternalProject_Add(
    wrapper_rootless_exe
    PREFIX ${CMAKE_BINARY_DIR}/wrapper_rootless_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND aarch64-linux-gnu-gcc -O3 -Wall -o wrapper_rootless ${WRAPPER_ROOTLESS_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    DEPENDS main
)

ExternalProject_Add(
    wrapper_android_exe
    PREFIX ${CMAKE_BINARY_DIR}/wrapper_android_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND aarch64-linux-gnu-gcc -O3 -Wall -o wrapper_android ${WRAPPER_ANDROID_SOURCE} ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    DEPENDS main
)