#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

enum { PSP_SYSTEMPARAM_LANGUAGE_ENGLISH=1, PSP_UTILITY_ACCEPT_CROSS=1,
       PSP_SYSTEMPARAM_ID_INT_LANGUAGE=8, PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP=9,
       PSP_NETCONF_ACTION_CONNECTAP=0, PSP_NET_APCTL_STATE_DISCONNECTED=0,
       PSP_NET_APCTL_STATE_GOT_IP=4, PSP_CTRL_CROSS=16384, PSP_CTRL_CIRCLE=8192,
       PSP_UTILITY_DIALOG_NONE=0, PSP_UTILITY_DIALOG_INIT=1,
       PSP_UTILITY_DIALOG_VISIBLE=2, PSP_UTILITY_DIALOG_QUIT=3,
       PSP_UTILITY_DIALOG_FINISHED=4, GU_DIRECT=0, GU_COLOR_BUFFER_BIT=1,
       GU_SYNC_FINISH=0, GU_SYNC_WHAT_DONE=0, PSP_NET_APCTL_INFO_IP=8, PORT=4646 };
typedef struct { int size, language, buttonSwap, graphicsThread, accessThread,
    fontThread, soundThread, result; } Common;
struct pspUtilityNetconfAdhoc { char name[8]; unsigned timeout; };
typedef struct { Common base; int action; struct pspUtilityNetconfAdhoc *adhocparam; } pspUtilityNetconfData;
typedef struct { unsigned Buttons; } SceCtrlData;
typedef union { char ip[16]; } SceNetApctlInfo;
static unsigned dialog_list[4096], wrong_pins;
static int server=-1;
static char ip[16], pin[7], network_error[96];
static const char *status;
static bool running=true;
static int init_result, dialog_result, final_state=4, info_result, socket_result=6,
           bind_result, polls, shutdowns, opened, disconnected, closed, frames=1500;
static bool finished;
static pspUtilityNetconfData *config_ptr;
static void dialog_graphics(void) { }
static int sceUtilityGetSystemParamInt(int id, int *out) { *out=id==8?1:1; return 0; }
static int sceNetApctlDisconnect(void) { ++disconnected; finished=false; return 0; }
static int sceNetApctlGetState(int *state) { *state=finished?final_state:0; return 0; }
static void sceKernelDelayThread(int us) { (void)us; }
static int sceUtilityNetconfInitStart(pspUtilityNetconfData *config) {
    assert(config->base.size==sizeof(*config));
    assert(config->action==PSP_NETCONF_ACTION_CONNECTAP);
    assert(config->adhocparam);
    config_ptr=config; polls=0; return init_result;
}
static int sceUtilityNetconfGetStatus(void) {
    ++polls;
    if (polls==1) return PSP_UTILITY_DIALOG_INIT;
    if (polls<=frames) return PSP_UTILITY_DIALOG_VISIBLE;
    if (polls==frames+1) return PSP_UTILITY_DIALOG_QUIT;
    assert(shutdowns>0);
    config_ptr->base.result=dialog_result;
    finished=true;
    return PSP_UTILITY_DIALOG_FINISHED;
}
static int sceUtilityNetconfShutdownStart(void) { ++shutdowns; return 0; }
static void sceUtilityNetconfUpdate(int n) { assert(n==1); }
static void sceGuStart(int mode, void *list) { (void)mode; (void)list; }
static void sceGuClearColor(unsigned n) { (void)n; }
static void sceGuClear(int n) { (void)n; }
static void sceGuFinish(void) { }
static void sceGuSync(int mode,int what) { (void)mode; (void)what; }
static void sceGuTerm(void) { }
static void sceGuSwapBuffers(void) { }
static void sceDisplayWaitVblankStart(void) { }
static void sceCtrlPeekBufferPositive(SceCtrlData *pad,int count) { (void)count; pad->Buttons=0; }
static int sceNetApctlGetInfo(int id,SceNetApctlInfo *info) { (void)id; strcpy(info->ip,"192.168.1.7"); return info_result; }
static uint32_t sceNetInetInetAddr(const char *address) { return inet_addr(address); }
static int sceNetInetSocket(int domain,int type,int protocol) { (void)domain; (void)type; (void)protocol; ++opened; return socket_result; }
static int sceNetInetSetsockopt(int fd,int level,int opt,const void *value,int len) { (void)fd; (void)level; (void)opt; (void)value; (void)len; return 0; }
static int sceNetInetBind(int fd,const struct sockaddr *addr,int len) { (void)fd; (void)addr; (void)len; assert(finished); return bind_result; }
static int sceNetInetListen(int fd,int backlog) { (void)fd; (void)backlog; return 0; }
static int sceNetInetClose(int fd) { (void)fd; ++closed; return 0; }
static uint64_t sceKernelGetSystemTimeWide(void) { return 123456; }
