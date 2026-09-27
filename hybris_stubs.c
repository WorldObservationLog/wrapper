/*
 * hybris_stubs.c — provides all symbols declared in import.h via android_dlsym.
 *
 * Compiled into the host-native wrapper (glibc x86-64).
 * hybris_init_libs() must be called once before any stub is used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include "hybris_types.h"

/* hybris public API (from libhybris-core.so) */
extern void *android_dlopen(const char *filename, int flag);
extern void *android_dlsym(void *handle, const char *symbol);
extern const char *android_dlerror(void);

#define RTLD_NOW_GLOBAL 0x102

static void *h_ssc = NULL;  /* libstoreservicescore.so */
static void *h_apm = NULL;  /* libandroidappmusic.so   */

/* ── vtable data arrays ──────────────────────────────────────────────────────
 * import.h declares these as "extern void *_ZTV...;" (single pointer).
 * main.c uses them as "&_ZTV... + 2" — taking the address of the symbol and
 * offsetting by 2 void* widths to reach vtable slot [2] (first virtual func).
 *
 * In the normal Android dlopen build the linker resolves the extern reference
 * so that &_ZTV... == the vtable address in the .so.  In the hybris build we
 * cannot alias Android memory, so we define each symbol as an 8-element array
 * and memcpy the vtable content from Android memory into it.  Then:
 *   &_ZTV... (from main.c's extern void* view) == &array[0]
 *   &_ZTV... + 2 == &array[2] == vtable slot [2]  ✓
 * ──────────────────────────────────────────────────────────────────────────── */
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE[8];
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE[8];
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE[8];
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE[8];

/* ── symbol resolver ─────────────────────────────────────────────────────── */
static void *sym(const char *name) {
    void *p = android_dlsym(h_ssc, name);
    if (!p) p = android_dlsym(h_apm, name);
    if (!p) { fprintf(stderr, "[hybris] FATAL: %s\n", name); abort(); }
    return p;
}

/* ── init: load Android libs, patch vtables ─────────────────────────────── */
/* Returns 0 on success, -1 if an Android lib cannot be loaded. It must not
 * exit: in library mode (libdrm-native.so inside the Go engine) that would
 * take the whole engine down instead of just disabling DRM. */
int hybris_init_libs(const char *lib64) {
    char path[512];

    /* pre-load implicit system libs so verneed checks pass */
    static const char *preload[] = { "libdl.so", "libc.so", "libm.so", NULL };
    for (int i = 0; preload[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", lib64, preload[i]);
        android_dlopen(path, RTLD_NOW_GLOBAL);
    }

    snprintf(path, sizeof(path), "%s/libstoreservicescore.so", lib64);
    h_ssc = android_dlopen(path, RTLD_NOW_GLOBAL);
    if (!h_ssc) { fprintf(stderr, "[hybris] cannot load libstoreservicescore.so: %s\n", android_dlerror()); return -1; }

    snprintf(path, sizeof(path), "%s/libandroidappmusic.so", lib64);
    h_apm = android_dlopen(path, RTLD_NOW_GLOBAL);
    if (!h_apm) { fprintf(stderr, "[hybris] cannot load libandroidappmusic.so: %s\n", android_dlerror()); return -1; }

    /* copy vtable content into our local arrays so &_ZTV... + 2 == vtable[2] */
#define PATCHV(arr, sym_name) do { \
    void *p = android_dlsym(h_ssc, sym_name); \
    if (!p) p = android_dlsym(h_apm, sym_name); \
    if (p) memcpy(arr, p, 8 * sizeof(void*)); \
    else fprintf(stderr, "[hybris] warning: vtable %s not found\n", sym_name); \
} while(0)

    PATCHV(_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE,
           "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE");
    PATCHV(_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE,
           "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE");
    PATCHV(_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE,
           "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE");
    PATCHV(_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE,
           "_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE");

    fprintf(stderr, "[hybris] libs loaded\n");
    return 0;
}

/* ── special cases ───────────────────────────────────────────────────────── */

/* curl: provided by system libcurl, no stub needed */

/* Android log → stderr */
int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio;
    char buf[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    fprintf(stderr, "[%s] %s\n", tag, buf);
    return 0;
}
int __android_log_write(int prio, const char *tag, const char *text) {
    (void)prio; fprintf(stderr, "[%s] %s\n", tag, text); return 0;
}

/* resolv: no-op on glibc */
void _resolv_set_nameservers_for_net(unsigned netid, const char **servers,
                                     int numservers, const char *domains) {
    (void)netid; (void)servers; (void)numservers; (void)domains;
}

/* ── Bionic-compatible std::function callback objects ────────────────────────
 *
 * SVPlaybackLeaseManagerC2 expects two Bionic std::__ndk1::function<> objects
 * passed by const reference.  Passing glibc std::function objects crashes
 * because the copy constructor calls through Bionic's vtable which doesn't
 * match glibc's internal layout.
 *
 * Bionic NDK r21 libc++ std::__ndk1::function<F> uses the OLD field order:
 *   __f_ comes FIRST, __buf_ comes SECOND (opposite of newer LLVM libc++):
 *
 *   std::function<F> (32 bytes):
 *   [0..7]:  __f_      = &__buf_[0] when SBO (= self + 8); NULL = empty
 *   [8..15]: __buf_[0] = vptr of __func  (= &vtab[2])
 *   [16..23]: __buf_[1] = fn pointer (the stored callable)
 *   [24..31]: __buf_[2] = allocator state (zeroed for stateless)
 *
 * SBO path: Bionic checks (void*)__f_ == &__buf_[0], i.e. self[0] == self+8.
 * If true it calls __f_->__clone(__base* dst) to copy __func into dest's __buf_.
 *
 * The vtable layout for __base<R(Args...)> (Bionic NDK r21, vptr → vtab[2]):
 *   vptr[0] = vtab[2] ~__base() regular destructor
 *   vptr[1] = vtab[3] ~__base() deleting destructor
 *   vptr[2] = vtab[4] __clone() const → heap copy
 *   vptr[3] = vtab[5] __clone(__base*) const → SBO placement copy
 *   vptr[4] = vtab[6] __destroy() → SBO destroy (no free)
 *   vptr[5] = vtab[7] __destroy_and_delete() → destroy + heap free
 *   vptr[6] = vtab[8] operator()(Args&&...) → invoke
 *
 * Virtual functions receive `self = &__buf_[0]` (the __func object), so:
 *   self[0] = vptr, self[1] = fn ptr, self[2] = allocator.
 * ──────────────────────────────────────────────────────────────────────────── */

/* callbacks exported from main.cpp */
extern void endLeaseCbExport(const int *code_ptr);
extern void pbErrCbExport(void *arg);

/* ── endLeaseCallback: std::function<void(const int&)> ─────────────────────*/

static void vf_el_destroy(void *self) { (void)self; }
static void vf_el_destroy_and_delete(void *self) { free(self); }
static void *vf_el_clone_heap(void *self) {
    void *copy = malloc(3 * sizeof(void *));
    memcpy(copy, self, 3 * sizeof(void *));
    return copy;
}
static void vf_el_clone_sbo(void *self, void *dst) {
    memcpy(dst, self, 3 * sizeof(void *));
}
static void vf_el_invoke(void *self, const int *code) {
    void (*fn)(const int *) = (void (*)(const int *))((void **)self)[1];
    fn(code);
}

static void *vtab_endlease[9] = {
    (void *)0,                      /* [0] offset-to-top */
    (void *)0,                      /* [1] RTTI          */
    (void *)vf_el_destroy,          /* [2] ~__base() regular */
    (void *)vf_el_destroy_and_delete, /* [3] ~__base() deleting */
    (void *)vf_el_clone_heap,       /* [4] __clone() → heap  */
    (void *)vf_el_clone_sbo,        /* [5] __clone(dst) → SBO */
    (void *)vf_el_destroy,          /* [6] __destroy()        */
    (void *)vf_el_destroy_and_delete, /* [7] __destroy_and_delete() */
    (void *)vf_el_invoke,           /* [8] operator()(const int&) */
};

/* 32-byte buffer; set up by hybris_init_callbacks() */
uint8_t endLeaseCallback[32];

/* ── pbErrCallback: std::function<void(const shared_ptr<StoreErrorCond>&)> ─*/

static void vf_pe_destroy(void *self) { (void)self; }
static void vf_pe_destroy_and_delete(void *self) { free(self); }
static void *vf_pe_clone_heap(void *self) {
    void *copy = malloc(3 * sizeof(void *));
    memcpy(copy, self, 3 * sizeof(void *));
    return copy;
}
static void vf_pe_clone_sbo(void *self, void *dst) {
    memcpy(dst, self, 3 * sizeof(void *));
}
static void vf_pe_invoke(void *self, void *arg) {
    void (*fn)(void *) = (void (*)(void *))((void **)self)[1];
    fn(arg);
}

static void *vtab_pberr[9] = {
    (void *)0,
    (void *)0,
    (void *)vf_pe_destroy,
    (void *)vf_pe_destroy_and_delete,
    (void *)vf_pe_clone_heap,
    (void *)vf_pe_clone_sbo,
    (void *)vf_pe_destroy,
    (void *)vf_pe_destroy_and_delete,
    (void *)vf_pe_invoke,
};

uint8_t pbErrCallback[32];

/* Call once from main() after init(), before SVPlaybackLeaseManagerC2.
 *
 * Bionic NDK r21 libc++ std::function<F> (32 bytes) OLD layout:
 *   [0..7]:  __f_   = &__buf_[0] = self+8  (SBO path: __f_ points into __buf_)
 *   [8..15]: __buf_[0] = vptr of __func (= &vtab[2])
 *   [16..23]: __buf_[1] = fn pointer
 *   [24..31]: __buf_[2] = allocator (zeroed for stateless)
 *
 * SBO check in Bionic: (void*)__f_ == &__buf_[0]  →  self[0] == self+8
 * Invoke: self->__f_->operator()(args)
 *         = obj at (self+8), vptr=*(self+8)=&vtab[2], call vptr[6]
 */
void hybris_init_callbacks(void) {
    uint8_t *el = endLeaseCallback;
    ((void **)el)[0] = el + 8;                    /* __f_ = &__buf_[0]    */
    ((void **)el)[1] = &vtab_endlease[2];          /* __func vptr          */
    ((void **)el)[2] = (void *)endLeaseCbExport;   /* fn ptr               */
    ((void **)el)[3] = NULL;                       /* allocator (zeroed)   */

    uint8_t *pe = pbErrCallback;
    ((void **)pe)[0] = pe + 8;
    ((void **)pe)[1] = &vtab_pberr[2];
    ((void **)pe)[2] = (void *)pbErrCbExport;
    ((void **)pe)[3] = NULL;
}

/* ── function stubs ──────────────────────────────────────────────────────── */

void _ZN20androidstoreservices30SVSubscriptionStatusMgrFactory6createEv(struct shared_ptr *out) {
    static void (*fn)(struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices30SVSubscriptionStatusMgrFactory6createEv");
    fn(out);
}

void _ZN20androidstoreservices27SVSubscriptionStatusMgrImpl33checkSubscriptionStatusFromSourceERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEERKNS_23SVSubscriptionStatusMgr26SVSubscriptionStatusSourceE(
    struct shared_ptr *a, void *b, struct shared_ptr *c, int *d) {
    static void (*fn)(struct shared_ptr*,void*,struct shared_ptr*,int*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices27SVSubscriptionStatusMgrImpl33checkSubscriptionStatusFromSourceERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEERKNS_23SVSubscriptionStatusMgr26SVSubscriptionStatusSourceE");
    fn(a,b,c,d);
}

void _ZN17storeservicescore14RequestContext24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(void *obj, union std_string *path) {
    static void (*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore14RequestContext24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    fn(obj,path);
}

void _ZN14FootHillConfig6configERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE(union std_string *cfg) {
    static void (*fn)(union std_string*) = NULL;
    if (!fn) fn = sym("_ZN14FootHillConfig6configERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
    fn(cfg);
}

void _ZNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEE11make_sharedIJRNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEEEES3_DpOT_(
    struct shared_ptr *out, union std_string *str) {
    static void (*fn)(struct shared_ptr*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEE11make_sharedIJRNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEEEES3_DpOT_");
    fn(out,str);
}

void _ZNSt6__ndk110shared_ptrIN20androidstoreservices28AndroidPresentationInterfaceEE11make_sharedIJEEES3_DpOT_(struct shared_ptr *out) {
    static void (*fn)(struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZNSt6__ndk110shared_ptrIN20androidstoreservices28AndroidPresentationInterfaceEE11make_sharedIJEEES3_DpOT_");
    fn(out);
}

void _ZN20androidstoreservices28AndroidPresentationInterface16setDialogHandlerEPFvlNSt6__ndk110shared_ptrIN17storeservicescore14ProtocolDialogEEENS2_INS_36AndroidProtocolDialogResponseHandlerEEEE(
    void *obj, void (*handler)(long, struct shared_ptr *, struct shared_ptr *)) {
    static void (*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices28AndroidPresentationInterface16setDialogHandlerEPFvlNSt6__ndk110shared_ptrIN17storeservicescore14ProtocolDialogEEENS2_INS_36AndroidProtocolDialogResponseHandlerEEEE");
    fn(obj,(void*)handler);
}

void _ZN20androidstoreservices28AndroidPresentationInterface21setCredentialsHandlerEPFvNSt6__ndk110shared_ptrIN17storeservicescore18CredentialsRequestEEENS2_INS_33AndroidCredentialsResponseHandlerEEEE(
    void *obj, void (*handler)(struct shared_ptr *, struct shared_ptr *)) {
    static void (*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices28AndroidPresentationInterface21setCredentialsHandlerEPFvNSt6__ndk110shared_ptrIN17storeservicescore18CredentialsRequestEEENS2_INS_33AndroidCredentialsResponseHandlerEEEE");
    fn(obj,(void*)handler);
}

void _ZN17storeservicescore14RequestContext24setPresentationInterfaceERKNSt6__ndk110shared_ptrINS_21PresentationInterfaceEEE(
    void *obj, struct shared_ptr *iface) {
    static void (*fn)(void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore14RequestContext24setPresentationInterfaceERKNSt6__ndk110shared_ptrINS_21PresentationInterfaceEEE");
    fn(obj,iface);
}

void _ZNSt6__ndk110shared_ptrIN17storeservicescore16AuthenticateFlowEE11make_sharedIJRNS0_INS1_14RequestContextEEEEEES3_DpOT_(
    struct shared_ptr *out, struct shared_ptr *ctx) {
    static void (*fn)(struct shared_ptr*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZNSt6__ndk110shared_ptrIN17storeservicescore16AuthenticateFlowEE11make_sharedIJRNS0_INS1_14RequestContextEEEEEES3_DpOT_");
    fn(out,ctx);
}

void _ZN17storeservicescore16AuthenticateFlow3runEv(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore16AuthenticateFlow3runEv");
    fn(obj);
}

struct shared_ptr *_ZNK17storeservicescore16AuthenticateFlow8responseEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore16AuthenticateFlow8responseEv");
    return fn(obj);
}

int _ZNK17storeservicescore20AuthenticateResponse12responseTypeEv(void *obj) {
    static int (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore20AuthenticateResponse12responseTypeEv");
    return fn(obj);
}

void _ZN22SVPlaybackLeaseManagerC2ERKNSt6__ndk18functionIFvRKiEEERKNS1_IFvRKNS0_10shared_ptrIN17storeservicescore19StoreErrorConditionEEEEEE(
    void *obj, void *endcb, void *errcb) {
    static void (*fn)(void*,void*,void*) = NULL;
    if (!fn) fn = sym("_ZN22SVPlaybackLeaseManagerC2ERKNSt6__ndk18functionIFvRKiEEERKNS1_IFvRKNS0_10shared_ptrIN17storeservicescore19StoreErrorConditionEEEEEE");
    fn(obj,endcb,errcb);
}

void _ZN22SVPlaybackLeaseManager25refreshLeaseAutomaticallyERKb(void *obj, uint8_t *flag) {
    static void (*fn)(void*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZN22SVPlaybackLeaseManager25refreshLeaseAutomaticallyERKb");
    fn(obj,flag);
}

void _ZN22SVPlaybackLeaseManager12requestLeaseERKb(void *obj, uint8_t *flag) {
    static void (*fn)(void*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZN22SVPlaybackLeaseManager12requestLeaseERKb");
    fn(obj,flag);
}

/* zero-arg functions */
void *_ZN21SVFootHillSessionCtrl8instanceEv() {
    static void *(*fn)(void) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl8instanceEv");
    return fn();
}
void *_ZN21SVFootHillSessionCtrl7destroyEv() {
    static void *(*fn)(void) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl7destroyEv");
    return fn();
}

void _ZN21SVFootHillSessionCtrl9cleanKeysERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE(
    void *obj, union std_string *key) {
    static void (*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl9cleanKeysERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
    fn(obj,key);
}

void _ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_(
    struct shared_ptr *ret, void *obj,
    union std_string *a, union std_string *b, union std_string *c, union std_string *d,
    union std_string *e, union std_string *f, union std_string *g, union std_string *h) {
    static void (*fn)(struct shared_ptr*,void*,
                      union std_string*,union std_string*,union std_string*,union std_string*,
                      union std_string*,union std_string*,union std_string*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_");
    fn(ret,obj,a,b,c,d,e,f,g,h);
}

void _ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb(
    struct shared_ptr *ret, void *obj, union std_string *ckc) {
    static void (*fn)(struct shared_ptr*,void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb");
    fn(ret,obj,ckc);
}

void _ZNSt6__ndk110shared_ptrI18SVFootHillPContextED2Ev(struct shared_ptr *sp) {
    static void (*fn)(struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZNSt6__ndk110shared_ptrI18SVFootHillPContextED2Ev");
    fn(sp);
}

void **_ZNK18SVFootHillPContext9kdContextEv(void *obj) {
    static void **(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK18SVFootHillPContext9kdContextEv");
    return fn(obj);
}

long NfcRKVnxuKZy04KWbdFu71Ou(void *a, uint32_t b, void *c, void *d, size_t e) {
    static long (*fn)(void*,uint32_t,void*,void*,size_t) = NULL;
    if (!fn) fn = sym("NfcRKVnxuKZy04KWbdFu71Ou");
    return fn(a,b,c,d,e);
}

void _ZN17storeservicescore22ProtocolDialogResponse17setSelectedButtonERKNSt6__ndk110shared_ptrINS_14ProtocolButtonEEE(
    void *obj, struct shared_ptr *btn) {
    static void (*fn)(void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore22ProtocolDialogResponse17setSelectedButtonERKNSt6__ndk110shared_ptrINS_14ProtocolButtonEEE");
    fn(obj,btn);
}

union std_string *_ZNK17storeservicescore14ProtocolDialog5titleEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore14ProtocolDialog5titleEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore14ProtocolDialog7messageEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore14ProtocolDialog7messageEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore18CredentialsRequest5titleEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore18CredentialsRequest5titleEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore18CredentialsRequest7messageEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore18CredentialsRequest7messageEv");
    return fn(obj);
}
uint8_t _ZNK17storeservicescore18CredentialsRequest28requiresHSA2VerificationCodeEv(void *obj) {
    static uint8_t (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore18CredentialsRequest28requiresHSA2VerificationCodeEv");
    return fn(obj);
}

void _ZN20androidstoreservices28AndroidPresentationInterface28handleProtocolDialogResponseERKlRKNSt6__ndk110shared_ptrIN17storeservicescore22ProtocolDialogResponseEEE(
    void *obj, long *j, struct shared_ptr *resp) {
    static void (*fn)(void*,long*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices28AndroidPresentationInterface28handleProtocolDialogResponseERKlRKNSt6__ndk110shared_ptrIN17storeservicescore22ProtocolDialogResponseEEE");
    fn(obj,j,resp);
}

void _ZN20androidstoreservices28AndroidPresentationInterface25handleCredentialsResponseERKNSt6__ndk110shared_ptrIN17storeservicescore19CredentialsResponseEEE(
    void *obj, struct shared_ptr *resp) {
    static void (*fn)(void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN20androidstoreservices28AndroidPresentationInterface25handleCredentialsResponseERKNSt6__ndk110shared_ptrIN17storeservicescore19CredentialsResponseEEE");
    fn(obj,resp);
}

void _ZN17storeservicescore22ProtocolDialogResponseC1Ev(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore22ProtocolDialogResponseC1Ev");
    fn(obj);
}
void _ZN17storeservicescore19CredentialsResponseC1Ev(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore19CredentialsResponseC1Ev");
    fn(obj);
}
void _ZN17storeservicescore19CredentialsResponse11setUserNameERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(void *obj, union std_string *s) {
    static void (*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore19CredentialsResponse11setUserNameERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    fn(obj,s);
}
void _ZN17storeservicescore19CredentialsResponse11setPasswordERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(void *obj, union std_string *s) {
    static void (*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore19CredentialsResponse11setPasswordERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    fn(obj,s);
}
void _ZN17storeservicescore19CredentialsResponse15setResponseTypeENS0_12ResponseTypeE(void *obj, int t) {
    static void (*fn)(void*,int) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore19CredentialsResponse15setResponseTypeENS0_12ResponseTypeE");
    fn(obj,t);
}

struct std_vector *_ZNK17storeservicescore14ProtocolDialog7buttonsEv(void *obj) {
    static struct std_vector *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore14ProtocolDialog7buttonsEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore14ProtocolButton5titleEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore14ProtocolButton5titleEv");
    return fn(obj);
}

void _ZN17storeservicescore10DeviceGUID8instanceEv(struct shared_ptr *out) {
    static void (*fn)(struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10DeviceGUID8instanceEv");
    fn(out);
}

void _ZN17storeservicescore10DeviceGUID9configureERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_RKjRKb(
    void *ret, void *obj, union std_string *id1, union std_string *id2,
    unsigned int *api, uint8_t *flag) {
    static void (*fn)(void*,void*,union std_string*,union std_string*,unsigned int*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10DeviceGUID9configureERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_RKjRKb");
    fn(ret,obj,id1,id2,api,flag);
}

uint8_t _ZN13mediaplatform26DebugLogEnabledForPriorityENS_11LogPriorityE(void) {
    /* always return 0 (disabled) — avoids spamming logs */
    return 0;
}

void _ZN17storeservicescore20RequestContextConfigC2Ev(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore20RequestContextConfigC2Ev");
    fn(obj);
}
void _ZN17storeservicescore20RequestContextConfig9setCPFlagEb(void *obj, uint8_t flag) {
    static void (*fn)(void*,uint8_t) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore20RequestContextConfig9setCPFlagEb");
    fn(obj,flag);
}

#define CFG_SETTER(name) \
void name(void *obj, union std_string *s) { \
    static void (*fn)(void*,union std_string*) = NULL; \
    if (!fn) fn = sym(#name); \
    fn(obj,s); \
}
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig20setBaseDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig19setClientIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig20setVersionIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig21setPlatformIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig17setProductVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig14setDeviceModelERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig15setBuildVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig19setLocaleIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig21setLanguageIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
CFG_SETTER(_ZN17storeservicescore20RequestContextConfig24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE)
#undef CFG_SETTER

void _ZN17storeservicescore14RequestContext4initERKNSt6__ndk110shared_ptrINS_20RequestContextConfigEEE(
    void *obj, void *unused, struct shared_ptr *cfg) {
    static void (*fn)(void*,void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore14RequestContext4initERKNSt6__ndk110shared_ptrINS_20RequestContextConfigEEE");
    fn(obj,unused,cfg);
}

void _ZN21RequestContextManager9configureERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEE(struct shared_ptr *ctx) {
    static void (*fn)(struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN21RequestContextManager9configureERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEE");
    fn(ctx);
}

struct shared_ptr *_ZN22SVPlaybackLeaseManager12requestAssetERKmRKNSt6__ndk16vectorINS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEENS7_IS9_EEEERKb(
    void *obj, void *unused, unsigned long *adamId, struct std_vector *flavors, uint8_t *offline) {
    static struct shared_ptr *(*fn)(void*,void*,unsigned long*,struct std_vector*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZN22SVPlaybackLeaseManager12requestAssetERKmRKNSt6__ndk16vectorINS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEENS7_IS9_EEEERKb");
    return fn(obj,unused,adamId,flavors,offline);
}

int _ZNK23SVPlaybackAssetResponse13hasValidAssetEv(void *obj) {
    static int (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK23SVPlaybackAssetResponse13hasValidAssetEv");
    return fn(obj);
}
struct shared_ptr *_ZNK23SVPlaybackAssetResponse13playbackAssetEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK23SVPlaybackAssetResponse13playbackAssetEv");
    return fn(obj);
}
int _ZNK23SVPlaybackAssetResponse9errorCodeEv(void *obj) {
    static int (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK23SVPlaybackAssetResponse9errorCodeEv");
    return fn(obj);
}
union std_string *_ZNK23SVPlaybackAssetResponse12errorMessageEv(void *obj, void *out) {
    static union std_string *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZNK23SVPlaybackAssetResponse12errorMessageEv");
    return fn(obj,out);
}

union std_string *_ZNK17storeservicescore13PlaybackAsset9URLStringEv(void *obj, uint8_t *out) {
    static union std_string *(*fn)(void*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PlaybackAsset9URLStringEv");
    return fn(obj,out);
}
union std_string *_ZNK17storeservicescore13PlaybackAsset7flavorEv(void *obj, void *out) {
    static union std_string *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PlaybackAsset7flavorEv");
    return fn(obj,out);
}
union std_string *_ZNK17storeservicescore13PlaybackAsset11downloadKeyEv(void *obj, void *out) {
    static union std_string *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PlaybackAsset11downloadKeyEv");
    return fn(obj,out);
}

union std_string *_ZNK17storeservicescore14RequestContext20storeFrontIdentifierERKNSt6__ndk110shared_ptrINS_6URLBagEEE(
    void *obj, void *unused, struct shared_ptr *urlbag) {
    static union std_string *(*fn)(void*,void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore14RequestContext20storeFrontIdentifierERKNSt6__ndk110shared_ptrINS_6URLBagEEE");
    return fn(obj,unused,urlbag);
}

void _ZN21SVFootHillSessionCtrl16resetAllContextsEv(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN21SVFootHillSessionCtrl16resetAllContextsEv");
    fn(obj);
}

void _ZN8FootHillC2ERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_(
    void *obj, union std_string *root, union std_string *lib) {
    static void (*fn)(void*,union std_string*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN8FootHillC2ERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_");
    fn(obj,root,lib);
}
void _ZN8FootHill24defaultContextIdentifierEv(void *obj) {
    static void (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN8FootHill24defaultContextIdentifierEv");
    fn(obj);
}

/* HTTPMessageC2: (obj, url*, method*) */
void *_ZN13mediaplatform11HTTPMessageC2ENSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES7_(
    void *obj, union std_string *url, union std_string *method) {
    static void *(*fn)(void*,union std_string*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN13mediaplatform11HTTPMessageC2ENSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES7_");
    return fn(obj,url,method);
}

void _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(
    void *obj, union std_string *name, union std_string *val) {
    static void (*fn)(void*,union std_string*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_");
    fn(obj,name,val);
}

void _ZN13mediaplatform11HTTPMessage11setBodyDataEPcm(void *obj, char *data, unsigned long len) {
    static void (*fn)(void*,char*,unsigned long) = NULL;
    if (!fn) fn = sym("_ZN13mediaplatform11HTTPMessage11setBodyDataEPcm");
    fn(obj,data,len);
}

void *_ZN17storeservicescore10DeviceGUID4guidEv(void *obj, void *out) {
    static void *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10DeviceGUID4guidEv");
    return fn(obj,out);
}

char *_ZNK13mediaplatform4Data5bytesEv(void *data) {
    static char *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK13mediaplatform4Data5bytesEv");
    return fn(data);
}

size_t _ZNK13mediaplatform4Data6lengthEv(void *data) {
    static size_t (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK13mediaplatform4Data6lengthEv");
    return fn(data);
}

void *_ZN17storeservicescore10URLRequestC2ERKNSt6__ndk110shared_ptrIN13mediaplatform11HTTPMessageEEERKNS2_INS_14RequestContextEEE(
    void *obj, struct shared_ptr *msg, struct shared_ptr *ctx) {
    static void *(*fn)(void*,struct shared_ptr*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10URLRequestC2ERKNSt6__ndk110shared_ptrIN13mediaplatform11HTTPMessageEEERKNS2_INS_14RequestContextEEE");
    return fn(obj,msg,ctx);
}

void *_ZN17storeservicescore10URLRequest19setRequestParameterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(
    void *obj, union std_string *key, union std_string *val) {
    static void *(*fn)(void*,union std_string*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10URLRequest19setRequestParameterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_");
    return fn(obj,key,val);
}

void *_ZN17storeservicescore10URLRequest3runEv(void *obj) {
    static void *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore10URLRequest3runEv");
    return fn(obj);
}
struct shared_ptr *_ZNK17storeservicescore10URLRequest5errorEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore10URLRequest5errorEv");
    return fn(obj);
}
struct shared_ptr *_ZNK17storeservicescore10URLRequest8responseEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore10URLRequest8responseEv");
    return fn(obj);
}
struct shared_ptr *_ZNK17storeservicescore11URLResponse18underlyingResponseEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore11URLResponse18underlyingResponseEv");
    return fn(obj);
}

void *_ZN17storeservicescore15PurchaseRequestC2ERKNSt6__ndk110shared_ptrINS_14RequestContextEEE(
    void *obj, struct shared_ptr *ctx) {
    static void *(*fn)(void*,struct shared_ptr*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore15PurchaseRequestC2ERKNSt6__ndk110shared_ptrINS_14RequestContextEEE");
    return fn(obj,ctx);
}
void *_ZN17storeservicescore15PurchaseRequest23setProcessDialogActionsEb(void *obj, int flag) {
    static void *(*fn)(void*,int) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore15PurchaseRequest23setProcessDialogActionsEb");
    return fn(obj,flag);
}
void *_ZN17storeservicescore15PurchaseRequest12setURLBagKeyERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(void *obj, union std_string *s) {
    static void *(*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore15PurchaseRequest12setURLBagKeyERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    return fn(obj,s);
}
void *_ZN17storeservicescore15PurchaseRequest16setBuyParametersERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(void *obj, union std_string *s) {
    static void *(*fn)(void*,union std_string*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore15PurchaseRequest16setBuyParametersERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE");
    return fn(obj,s);
}
void *_ZN17storeservicescore15PurchaseRequest3runEv(void *obj) {
    static void *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore15PurchaseRequest3runEv");
    return fn(obj);
}
struct shared_ptr *_ZNK17storeservicescore15PurchaseRequest8responseEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore15PurchaseRequest8responseEv");
    return fn(obj);
}
struct shared_ptr *_ZN17storeservicescore16PurchaseResponse5errorEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore16PurchaseResponse5errorEv");
    return fn(obj);
}
struct std_vector _ZNK17storeservicescore16PurchaseResponse5itemsEv(void *obj) {
    static struct std_vector (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore16PurchaseResponse5itemsEv");
    return fn(obj);
}
struct std_vector _ZNK17storeservicescore12PurchaseItem6assetsEv(void *obj) {
    static struct std_vector (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore12PurchaseItem6assetsEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore13PurchaseAsset3URLEv(void *obj, void *out) {
    static union std_string *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PurchaseAsset3URLEv");
    return fn(obj,out);
}
union std_string *_ZNK17storeservicescore13PurchaseAsset11downloadKeyEv(void *obj, void *out) {
    static union std_string *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PurchaseAsset11downloadKeyEv");
    return fn(obj,out);
}
int _ZNK17storeservicescore19StoreErrorCondition9errorCodeEv(void *obj) {
    static int (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore19StoreErrorCondition9errorCodeEv");
    return fn(obj);
}
const char *_ZNK17storeservicescore19StoreErrorCondition4whatEv(void *obj) {
    static const char *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore19StoreErrorCondition4whatEv");
    return fn(obj);
}
struct shared_ptr *_ZNK17storeservicescore20AuthenticateResponse5errorEv(void *obj) {
    static struct shared_ptr *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore20AuthenticateResponse5errorEv");
    return fn(obj);
}
union std_string *_ZNK17storeservicescore20AuthenticateResponse15customerMessageEv(void *obj) {
    static union std_string *(*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore20AuthenticateResponse15customerMessageEv");
    return fn(obj);
}
void *_ZN17storeservicescore14RequestContext8fairPlayEv(void *obj, void *out) {
    static void *(*fn)(void*,void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore14RequestContext8fairPlayEv");
    return fn(obj,out);
}
struct std_vector _ZN17storeservicescore8FairPlay21getSubscriptionStatusEv(void *obj) {
    static struct std_vector (*fn)(void*) = NULL;
    if (!fn) fn = sym("_ZN17storeservicescore8FairPlay21getSubscriptionStatusEv");
    return fn(obj);
}

/* itun FairPlay decryption */
struct std_vector *_ZNK17storeservicescore13PlaybackAsset5sinfsEv(
    struct std_vector *ret, void *playbackAsset) {
    static struct std_vector *(*fn)(struct std_vector*,void*) = NULL;
    if (!fn) fn = sym("_ZNK17storeservicescore13PlaybackAsset5sinfsEv");
    return fn(ret,playbackAsset);
}

struct shared_ptr *_ZN18SVDecryptorFactory6createERKN11SVDecryptor15SVDecryptorTypeEPKhRKjS5_S7_RKNS0_20SVDecryptorTrackTypeERKbSC_(
    struct shared_ptr *ret, int *protType, const uint8_t *keyData, uint32_t *keyLen,
    const uint8_t *ivData, uint32_t *ivLen, int *trackType, uint8_t *b1, uint8_t *b2) {
    static struct shared_ptr *(*fn)(struct shared_ptr*,int*,const uint8_t*,uint32_t*,
                                    const uint8_t*,uint32_t*,int*,uint8_t*,uint8_t*) = NULL;
    if (!fn) fn = sym("_ZN18SVDecryptorFactory6createERKN11SVDecryptor15SVDecryptorTypeEPKhRKjS5_S7_RKNS0_20SVDecryptorTrackTypeERKbSC_");
    return fn(ret,protType,keyData,keyLen,ivData,ivLen,trackType,b1,b2);
}

void _ZN17SVPastisDecryptor13decryptSampleEPKhRKjPj(
    void *decryptor, const uint8_t *data, const uint32_t *len, uint32_t *outLen) {
    static void (*fn)(void*,const uint8_t*,const uint32_t*,uint32_t*) = NULL;
    if (!fn) fn = sym("_ZN17SVPastisDecryptor13decryptSampleEPKhRKjPj");
    fn(decryptor,data,len,outLen);
}
