#include "internal.h"
#include "cJSON.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

void dialogHandler(long j, struct shared_ptr* protoDialogPtr,
                   struct shared_ptr* respHandler) {
    (void)respHandler;
    const char* const title = std_string_data(
        _ZNK17storeservicescore14ProtocolDialog5titleEv(protoDialogPtr->obj));
    LOG_INFO("dialogHandler: {title: %s, message: %s}", title,
             std_string_data(_ZNK17storeservicescore14ProtocolDialog7messageEv(protoDialogPtr->obj)));

    unsigned char ptr[72];
    memset(ptr + 8, 0, 16);
    *(void**)(ptr) = &_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE + 2;
    struct shared_ptr diagResp = {.obj = ptr + 24, .ctrl_blk = ptr};
    _ZN17storeservicescore22ProtocolDialogResponseC1Ev(diagResp.obj);

    struct std_vector* butVec = _ZNK17storeservicescore14ProtocolDialog7buttonsEv(protoDialogPtr->obj);
    if (strcmp("Sign In", title) == 0) {
        int found = 0;
        for (struct shared_ptr* b = (struct shared_ptr*)butVec->begin;
             b != (struct shared_ptr*)butVec->end; ++b) {
            LOG_DEBUG("signin button: %s",
                      std_string_data(_ZNK17storeservicescore14ProtocolButton5titleEv(b->obj)));
            if (strcmp("Use Existing Apple ID",
                       std_string_data(_ZNK17storeservicescore14ProtocolButton5titleEv(b->obj))) == 0) {
                _ZN17storeservicescore22ProtocolDialogResponse17setSelectedButtonERKNSt6__ndk110shared_ptrINS_14ProtocolButtonEEE(diagResp.obj, b);
                found = 1;
                break;
            }
        }
        LOG_DEBUG("signin button selected: %s", found ? "yes" : "NO");
    } else {
        for (struct shared_ptr* b = (struct shared_ptr*)butVec->begin;
             b != (struct shared_ptr*)butVec->end; ++b) {
            LOG_INFO("button %p: %s", b->obj,
                     std_string_data(_ZNK17storeservicescore14ProtocolButton5titleEv(b->obj)));
        }
    }
    _ZN20androidstoreservices28AndroidPresentationInterface28handleProtocolDialogResponseERKlRKNSt6__ndk110shared_ptrIN17storeservicescore22ProtocolDialogResponseEEE(
        apInf.obj, &j, &diagResp);
}

void credentialHandler(struct shared_ptr* credReqPtr,
                       struct shared_ptr* credRespHandler) {
    (void)credRespHandler;
    const uint8_t need2FA =
        _ZNK17storeservicescore18CredentialsRequest28requiresHSA2VerificationCodeEv(credReqPtr->obj);
    LOG_INFO("credentialHandler: {title: %s, message: %s, 2FA: %s}",
             std_string_data(_ZNK17storeservicescore18CredentialsRequest5titleEv(credReqPtr->obj)),
             std_string_data(_ZNK17storeservicescore18CredentialsRequest7messageEv(credReqPtr->obj)),
             need2FA ? "true" : "false");

    int passLen = amPassword ? (int)strlen(amPassword) : 0;

    if (need2FA) {
        std::string path = std::string(g_base_dir) + "/2fa.txt";
        bool got_code = false;
        if (!g_code_from_file && isatty(STDIN_FILENO)) {
            char code[7];
            printf("2FA code: ");
            fflush(stdout);
            if (scanf("%6s", code) == 1 && amPassword) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s%s", amPassword, code);
                free(amPassword);
                amPassword = strdup(tmp);
                got_code = true;
            }
        }
        if (!got_code) {
            if (!file_exists(path.c_str())) {
                LOG_WARN("Enter your 2FA code into %s/2fa.txt", g_base_dir);
                int count = 0;
                while (!file_exists(path.c_str()) && count < 20) {
                    sleep(3);
                    count++;
                }
                if (!file_exists(path.c_str())) {
                    LOG_WARN("2FA code timeout (60s), aborting login");
                    exit(1);
                }
            }
            FILE* fp = fopen(path.c_str(), "r");
            if (fp) {
                if (amPassword) {
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "%s", amPassword);
                    if (fscanf(fp, "%6s", tmp + passLen) == 1) {
                        free(amPassword);
                        amPassword = strdup(tmp);
                    }
                }
                fclose(fp);
                remove(path.c_str());
                LOG_WARN("Code file detected! Logging in...");
            } else {
                LOG_WARN("Failed to open 2fa.txt");
            }
        }
    }

    uint8_t* ptr = (uint8_t*)malloc(80);
    memset(ptr + 8, 0, 16);
    *(void**)(ptr) = &_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE + 2;
    struct shared_ptr credResp = {.obj = ptr + 24, .ctrl_blk = ptr};
    _ZN17storeservicescore19CredentialsResponseC1Ev(credResp.obj);

    union std_string username = new_std_string(amUsername ? amUsername : "");
    _ZN17storeservicescore19CredentialsResponse11setUserNameERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(credResp.obj, &username);
    union std_string password = new_std_string(amPassword ? amPassword : "");
    _ZN17storeservicescore19CredentialsResponse11setPasswordERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(credResp.obj, &password);
    LOG_DEBUG("cred submit: user=%.32s len=%zu pass_len=%zu",
              amUsername ? amUsername : "(null)", username.size,
              amPassword ? strlen(amPassword) : 0);

    _ZN17storeservicescore19CredentialsResponse15setResponseTypeENS0_12ResponseTypeE(credResp.obj, 2);
    _ZN20androidstoreservices28AndroidPresentationInterface25handleCredentialsResponseERKNSt6__ndk110shared_ptrIN17storeservicescore19CredentialsResponseEEE(apInf.obj, &credResp);
}

void set_credentials(const char* user, const char* pass) {
    if (amUsername) free(amUsername);
    if (amPassword) free(amPassword);
    amUsername = (user) ? strdup(user) : nullptr;
    amPassword = (pass) ? strdup(pass) : nullptr;
}

bool login(struct shared_ptr ctx) {
    LOG_INFO("logging in...");
    std::string storefrontPath = std::string(g_base_dir) + "/STOREFRONT_ID";
    std::string musicTokenPath = std::string(g_base_dir) + "/MUSIC_TOKEN";
    if (file_exists(storefrontPath.c_str())) remove(storefrontPath.c_str());
    if (file_exists(musicTokenPath.c_str())) remove(musicTokenPath.c_str());

    struct shared_ptr flow;
    memset(&flow, 0, sizeof(flow));
    _ZNSt6__ndk110shared_ptrIN17storeservicescore16AuthenticateFlowEE11make_sharedIJRNS0_INS1_14RequestContextEEEEEES3_DpOT_(&flow, &ctx);
    _ZN17storeservicescore16AuthenticateFlow3runEv(flow.obj);
    struct shared_ptr* resp = _ZNK17storeservicescore16AuthenticateFlow8responseEv(flow.obj);
    if (!resp || !resp->obj) return false;
    int respType = _ZNK17storeservicescore20AuthenticateResponse12responseTypeEv(resp->obj);
    if (respType != 6) {
        const char* customer_msg = std_string_data(
            _ZNK17storeservicescore20AuthenticateResponse15customerMessageEv(resp->obj));
        if (customer_msg && *customer_msg) {
            LOG_WARN("server message: %s", customer_msg);
        }
        struct shared_ptr* err = _ZNK17storeservicescore20AuthenticateResponse5errorEv(resp->obj);
        if (err && err->obj) {
            int code = _ZNK17storeservicescore19StoreErrorCondition9errorCodeEv(err->obj);
            const char* what = _ZNK17storeservicescore19StoreErrorCondition4whatEv(err->obj);
            LOG_WARN("auth error: code=%d, message=%s", code, what ? what : "none");
        } else {
            LOG_WARN("auth failed: response type %d", respType);
        }
        return false;
    }
    return true;
}
