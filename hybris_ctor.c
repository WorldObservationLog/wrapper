/*
 * hybris_ctor.c — automatic pre-main init for the host-native DRM wrapper.
 *
 * The constructor runs before main() and loads the Android libs via hybris.
 * It reads the lib64 path from HYBRIS_ANDROID_LIB64, which the Go engine
 * must set before exec()'ing the wrapper process.
 *
 * Build with: gcc -c hybris_ctor.c -o hybris_ctor.o
 * Link into the final binary alongside main.o and hybris_stubs.o.
 */

#include <stdio.h>
#include <stdlib.h>

extern void hybris_init_libs(const char *lib64_path);

__attribute__((constructor))
static void hybris_auto_init(void) {
    const char *lib64 = getenv("HYBRIS_ANDROID_LIB64");
    if (!lib64) {
        fprintf(stderr, "[hybris] HYBRIS_ANDROID_LIB64 not set — Android libs not loaded\n");
        return;
    }
    hybris_init_libs(lib64);
}
