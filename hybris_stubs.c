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

/* ── vtable data globals ─────────────────────────────────────────────────── */
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE = NULL;
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE = NULL;
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE = NULL;
void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE = NULL;

/* ── symbol resolver ─────────────────────────────────────────────────────── */
static void *sym(const char *name) {
    void *p = android_dlsym(h_ssc, name);
    if (!p) p = android_dlsym(h_apm, name);
    if (!p) { fprintf(stderr, "[hybris] FATAL: %s\n", name); abort(); }
    return p;
}

/* ── init: load Android libs, patch vtables ─────────────────────────────── */
void hybris_init_libs(const char *lib64) {
    char path[512];

    /* pre-load implicit system libs so verneed checks pass */
    static const char *preload[] = { "libdl.so", "libc.so", "libm.so", NULL };
    for (int i = 0; preload[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", lib64, preload[i]);
        android_dlopen(path, RTLD_NOW_GLOBAL);
    }

    snprintf(path, sizeof(path), "%s/libstoreservicescore.so", lib64);
    h_ssc = android_dlopen(path, RTLD_NOW_GLOBAL);
    if (!h_ssc) { fprintf(stderr, "[hybris] cannot load libstoreservicescore.so: %s\n", android_dlerror()); exit(1); }

    snprintf(path, sizeof(path), "%s/libandroidappmusic.so", lib64);
    h_apm = android_dlopen(path, RTLD_NOW_GLOBAL);
    if (!h_apm) { fprintf(stderr, "[hybris] cannot load libandroidappmusic.so: %s\n", android_dlerror()); exit(1); }

    /* patch vtable pointers */
#define PATCHV(sym_name) do { \
    void *p = android_dlsym(h_ssc, sym_name); \
    if (!p) p = android_dlsym(h_apm, sym_name); \
    if (p) sym_name ## _ptr = p; \
    else fprintf(stderr, "[hybris] warning: vtable %s not found\n", sym_name); \
} while(0)

    /* use local pointer trick since macro won't work with long names */
    void *p;
    p = android_dlsym(h_ssc, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE");
    if (!p) p = android_dlsym(h_apm, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE");
    if (p) _ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE = p;

    p = android_dlsym(h_ssc, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE");
    if (!p) p = android_dlsym(h_apm, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE");
    if (p) _ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE = p;

    p = android_dlsym(h_ssc, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE");
    if (!p) p = android_dlsym(h_apm, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE");
    if (p) _ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE = p;

    p = android_dlsym(h_ssc, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE");
    if (!p) p = android_dlsym(h_apm, "_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE");
    if (p) _ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE = p;

    fprintf(stderr, "[hybris] libs loaded\n");
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
