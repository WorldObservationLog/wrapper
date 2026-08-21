#pragma once
#include "lite.h"
#include "import.h"
#include "logger.h"
#include <mutex>

extern void* FHinstance;
extern struct shared_ptr g_reqCtx;
extern std::mutex g_playback_mutex;
extern std::mutex g_token_mutex;
extern char g_base_dir[256];
extern int offlineFlag;
extern uint8_t leaseMgr[256];
extern struct shared_ptr apInf;
extern struct shared_ptr GUID;
extern char* amUsername;
extern char* amPassword;
extern char* device_infos[9];

int file_exists(const char* path);
void set_credentials(const char* user, const char* pass);
void dialogHandler(long j, struct shared_ptr* protoDialogPtr, struct shared_ptr* respHandler);
void credentialHandler(struct shared_ptr* credReqPtr, struct shared_ptr* credRespHandler);
void init(const char* device_info_str);
struct shared_ptr init_ctx();
void install_hooks();
bool login(struct shared_ptr ctx);
void setup_services();
int offline_available_impl();

const char* get_m3u8_download(unsigned long adam);
const char* get_m3u8_play(unsigned long adam);
