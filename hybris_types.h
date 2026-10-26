#pragma once
/* Minimal struct definitions for hybris_stubs.c — does NOT include the
 * const data (android_id, fairplayCert) that import.h defines, which
 * would cause duplicate-symbol errors when linked with main.o. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct shared_ptr {
    void *obj;
    void *ctrl_blk;
};

union std_string {
    struct {
        uint8_t mark;
        char str[0];
    };
    struct {
        size_t cap;
        size_t size;
        const char *data;
    };
};

struct std_vector {
    void *begin;
    void *end;
    void *end_capacity;
};

struct FairPlayData {
    const uint8_t *bytes_ptr;
    uint32_t dataType;
    uint32_t length;
};

struct FairPlaySinf {
    int64_t identifier;
    struct shared_ptr dpInfoData;
    struct shared_ptr sinfData;
    struct shared_ptr sinf2Data;
};
