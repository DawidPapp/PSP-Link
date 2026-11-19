#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet.h>
#include <psputility.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "update_format.h"

PSP_MODULE_INFO("PSP Link", PSP_MODULE_USER, 1, 5);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(128);
PSP_HEAP_SIZE_KB(16384);


enum { PORT = 4646, LIMIT = 1024 * 1024, APP_LIMIT = 32 * 1024 * 1024, STRIDE = 512, HEIGHT = 272 };
#define ROOT "ms0:/PSP/GAME/PSPLink/Transfer/"
#define PART ROOT ".incoming"
#define GAME "ms0:/PSP/GAME/"
#define WHITE 0xfff7fbffu
#define MUTED 0xffc0b098u

static uint32_t *vram;
static volatile uint32_t *pixels;
static unsigned frame, received, sent, wrong_pins;
static int server = -1, category, row = 1;
static char update_title[64]="Sprawdź aktualizacje", update_detail[96]=DEFAULT_REPOSITORY;
static char ip[16], pin[7], network_error[96];
static unsigned int dialog_list[4096] __attribute__((aligned(16)));
static const char *status = "Wybierz Połącz z Wi-Fi, aby rozpocząć.";
static volatile bool running = true;

static int exit_callback(int a, int b, void *p)
{
    (void)a; (void)b; (void)p;
    running = false;
    return 0;
}

static int callbacks(SceSize size, void *arg)
{
    (void)size; (void)arg;
    int callback = sceKernelCreateCallback("Exit", exit_callback, NULL);
    if (callback >= 0) {
        sceKernelRegisterExitCallback(callback);
        sceKernelSleepThreadCB();
    }
    return 0;
}

#include "ui/menu.h"

/* poll/select works in both the PSP networking stack and the emulator. */
static bool ready(int fd, bool writing, int seconds)
{
    if (!running || fd < 0 || fd >= FD_SETSIZE) return false;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    SceNetInetTimeval timeout = {seconds, 0};
    int result = sceNetInetSelect(fd + 1, writing ? NULL : &set,
                                 writing ? &set : NULL, NULL, &timeout);
    return result > 0;
}

static bool wire(int fd, void *data, size_t n, bool sending)
{
    unsigned char *p = data;
    while (n && running) {
        if (!ready(fd, sending, 8)) return false;
        int moved = sending ? (int)sceNetInetSend(fd, p, n, 0)
                            : (int)sceNetInetRecv(fd, p, n, 0);
        if (moved <= 0) return false;
        p += moved;
        n -= moved;
    }
    return n == 0;
}

static bool reply(int fd, const char *s)
{
    return wire(fd, (void *)s, strlen(s), true);
}

static bool request_line(int fd, char *out, size_t cap)
{
    uint32_t start = sceKernelGetSystemTimeLow();
    for (size_t i = 0; i + 1 < cap; ++i) {
        if (sceKernelGetSystemTimeLow() - start > 10000000 ||
            !wire(fd, out + i, 1, false)) return false;
        if (out[i] == '\n') {
            out[i] = 0;
            return true;
        }
        if ((unsigned char)out[i] < 32 || (unsigned char)out[i] > 126) return false;
    }
    return false;
}

static bool file_name(const char *name)
{
    size_t n = strlen(name);
    if (n < 5 || n > 40 || name[0] == '.' || strcmp(name + n - 4, ".txt"))
        return false;
    for (; *name; ++name) {
        char c = *name;
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '-' && c != '_' && c != '.')
            return false;
    }
    return true;
}

static void list_files(int fd)
{
    SceUID dir = sceIoDopen(ROOT);
    if (dir < 0) {
        reply(fd, "ERR Brak folderu Transfer\n");
        return;
    }
    SceIoDirent entry = {0};
    char line[43];
    bool ok = reply(fd, "OK\n");
    int result = 0;
    while (ok && (result = sceIoDread(dir, &entry)) > 0) {
        entry.d_name[sizeof(entry.d_name) - 1] = 0;
        if (!FIO_S_ISREG(entry.d_stat.st_mode) || !file_name(entry.d_name)) continue;
        snprintf(line, sizeof(line), "%.40s\n", entry.d_name);
        ok = reply(fd, line);
    }
    sceIoDclose(dir);
    if (ok && result == 0) reply(fd, ".\n");
}

static void download(int fd, const char *path)
{
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0 || !FIO_S_ISREG(st.st_mode) ||
        st.st_size < 0 || st.st_size > LIMIT) {
        reply(fd, "ERR Brak pliku lub plik jest za duzy\n");
        return;
    }
    SceUID f = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (f < 0) {
        reply(fd, "ERR Nie mozna otworzyc pliku\n");
        return;
    }
    char header[32], bytes[4096];
    unsigned remaining = (unsigned)st.st_size;
    snprintf(header, sizeof(header), "OK %u\n", remaining);
    bool ok = reply(fd, header);
    while (ok && remaining) {
        unsigned chunk = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        int count = sceIoRead(f, bytes, chunk);
        if (count <= 0 || !wire(fd, bytes, count, true)) {
            ok = false;
            break;
        }
        remaining -= count;
    }
    sceIoClose(f);
    if (ok) ++sent;
}

/* Folder names are one ASCII component, never a path. */
static bool app_name(const char *name)
{
    size_t n = strlen(name);
    if (!n || n > 40 || !strcasecmp(name, "PSPLink")) return false;
    for (; *name; ++name) {
        char c = *name;
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '-' && c != '_') return false;
    }
    return true;
}

/* PBP header: signature, version, eight little-endian section offsets.
   This validates the container, not its signature or OFW compatibility. */
static bool pbp_header(const unsigned char *bytes, unsigned size)
{
    if (size < 40 || memcmp(bytes, "\0PBP", 4)) return false;
    uint32_t previous = 40;
    for (unsigned i = 8; i < 40; i += 4) {
        uint32_t offset = (uint32_t)bytes[i] | (uint32_t)bytes[i+1] << 8 |
                          (uint32_t)bytes[i+2] << 16 | (uint32_t)bytes[i+3] << 24;
        if (offset < previous || offset > size) return false;
        previous = offset;
    }
    return true;
}

static void upload(int fd, const char *path, unsigned remaining, const char *appdir)
{
    SceIoStat st;
    if (sceIoGetstat(appdir ? appdir : path, &st) >= 0) {
        reply(fd, "ERR Plik lub folder juz istnieje. Wybierz inna nazwe\n");
        return;
    }
    /* Rename only within one directory: PSP Memory Stick is not POSIX rename. */
    char temporary[sizeof(GAME) + 64];
    bool made_dir = false;
    if (appdir) {
        if (sceIoMkdir(appdir, 0777) < 0) {
            reply(fd, "ERR Nie mozna utworzyc folderu gry\n");
            return;
        }
        made_dir = true;
        snprintf(temporary, sizeof(temporary), "%s/LINK.TMP", appdir);
    } else {
        snprintf(temporary, sizeof(temporary), "%s", PART);
    }
    SceUID f = sceIoOpen(temporary, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (f < 0) {
        if (made_dir) sceIoRmdir(appdir);
        reply(fd, "ERR Nie mozna zapisac na karcie\n");
        return;
    }
    unsigned total = remaining;
    char bytes[4096];
    bool first = true, ok = reply(fd, "GO\n");
    const char *error = "ERR Zapis przerwany lub brak miejsca\n";
    while (ok && remaining) {
        unsigned chunk = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        if (!wire(fd, bytes, chunk, false)) {
            ok = false;
            break;
        }
        if (first && appdir && !pbp_header((unsigned char *)bytes, total)) {
            error = "ERR Niepoprawny naglowek PBP\n";
            ok = false;
            break;
        }
        first = false;
        unsigned offset = 0;
        while (offset < chunk) {
            int written = sceIoWrite(f, bytes + offset, chunk - offset);
            if (written <= 0) break;
            offset += written;
        }
        if (offset != chunk) {
            ok = false;
            break;
        }
        remaining -= chunk;
    }
    int closed = sceIoClose(f);
    bool renamed = false;
    if (ok && closed >= 0) {
        renamed = sceIoRename(temporary, path) >= 0;
        if (!renamed) error = "ERR Nie udalo sie nadac nazwy pliku docelowego\n";
        ok = renamed;
    } else {
        ok = false;
    }
    /* A successful rename return alone is not proof the file reached its target. */
    if (ok) {
        ok = sceIoGetstat(path, &st) >= 0 && FIO_S_ISREG(st.st_mode) &&
             st.st_size == total;
        if (ok && appdir) {
            unsigned char header[40];
            SceUID check = sceIoOpen(path, PSP_O_RDONLY, 0);
            if (check < 0) ok = false;
            else {
                ok = sceIoRead(check, header, sizeof(header)) == sizeof(header) &&
                     pbp_header(header, total);
                if (sceIoClose(check) < 0) ok = false;
            }
        }
        if (!ok) error = "ERR Brak poprawnego pliku w folderze docelowym\n";
    }
    if (!ok) {
        sceIoRemove(temporary);
        if (renamed) sceIoRemove(path);
        if (made_dir) sceIoRmdir(appdir);
        reply(fd, error);
        return;
    }
    ++received;
    reply(fd, "OK\n");
}

static void serve(int fd)
{
    char line[96], code[7], cmd[5], name[41], extra;
    unsigned size;
    if (!request_line(fd, line, sizeof(line))) return;
    if (sscanf(line, "%6[0-9] %4[A-Z] %40s %u %c",
               code, cmd, name, &size, &extra) != 4) {
        reply(fd, "ERR Niepoprawne polecenie\n");
        return;
    }
    if (wrong_pins >= 5 || strcmp(code, pin)) {
        if (wrong_pins < 5) ++wrong_pins;
        reply(fd, "ERR PIN lub blokada sesji\n");
        return;
    }
    if (!strcmp(cmd, "LIST") && !strcmp(name, "-") && size == 0) {
        list_files(fd);
        return;
    }
    if (!strcmp(cmd, "APP")) {
        if (!app_name(name) || size < 40 || size > APP_LIMIT) {
            reply(fd, "ERR Niepoprawny folder lub rozmiar PBP\n");
            return;
        }
        char dir[sizeof(GAME) + 40], apppath[sizeof(GAME) + 52];
        snprintf(dir, sizeof(dir), GAME "%s", name);
        snprintf(apppath, sizeof(apppath), "%s/EBOOT.PBP", dir);
        upload(fd, apppath, size, dir);
        return;
    }
    if (!file_name(name) || size > LIMIT) {
        reply(fd, "ERR Niepoprawna nazwa lub rozmiar\n");
        return;
    }
    char path[sizeof(ROOT) + 40];
    snprintf(path, sizeof(path), ROOT "%s", name);
    if (!strcmp(cmd, "GET") && size == 0) download(fd, path);
    else if (!strcmp(cmd, "PUT")) upload(fd, path, size, NULL);
    else reply(fd, "ERR Nieznane polecenie\n");
}

static void disconnect(void)
{
    if (server >= 0) sceNetInetClose(server);
    server = -1;
    sceNetApctlDisconnect();
    status = "Rozlaczono.";
}

/* A separate GU frame loop is needed for the system utility overlay.
   The normal XMB-style screen uses direct framebuffer writes. */
static void dialog_graphics(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, dialog_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, STRIDE);
    sceGuDispBuffer(480, HEIGHT, (void *)(STRIDE * HEIGHT * 4), STRIDE);
    sceGuOffset(2048 - 240, 2048 - HEIGHT / 2);
    sceGuViewport(2048, 2048, 480, HEIGHT);
    sceGuScissor(0, 0, 480, HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void net_error(const char *stage, int result)
{
    snprintf(network_error, sizeof(network_error), "%s: 0x%08X", stage, (unsigned)result);
    status = network_error;
}

static void connect_wifi(void)
{
    /* All configuration lives until the utility has completed shutdown. */
    static pspUtilityNetconfData config;
    static struct pspUtilityNetconfAdhoc adhoc;
    memset(&config, 0, sizeof(config));
    memset(&adhoc, 0, sizeof(adhoc));
    config.base.size = sizeof(config);
    config.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    config.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &config.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &config.base.buttonSwap);
    config.base.graphicsThread = 17;
    config.base.accessThread = 19;
    config.base.fontThread = 18;
    config.base.soundThread = 16;
    config.action = PSP_NETCONF_ACTION_CONNECTAP;
    config.adhocparam = &adhoc;
    /* Clear any association left by a failed attempt before opening selection. */
    sceNetApctlDisconnect();
    for (int i = 0; i < 100; ++i) {
        int state = 0;
        if (sceNetApctlGetState(&state) < 0 || state == PSP_NET_APCTL_STATE_DISCONNECTED) break;
        sceKernelDelayThread(20000);
    }
    dialog_graphics();
    int result = sceUtilityNetconfInitStart(&config);
    if (result < 0) {
        sceGuTerm();
        net_error("Blad okna Wi-Fi", result);
        return;
    }
    bool closing = false, done = false;
    while (running && !done) {
        sceGuStart(GU_DIRECT, dialog_list);
        sceGuClearColor(0xff48250d);
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        int state = sceUtilityNetconfGetStatus();
        switch (state) {
        case PSP_UTILITY_DIALOG_VISIBLE:
            sceUtilityNetconfUpdate(1);
            break;
        case PSP_UTILITY_DIALOG_QUIT:
            if (!closing) {
                result = sceUtilityNetconfShutdownStart();
                closing = result >= 0;
                if (!closing) done = true;
            }
            break;
        case PSP_UTILITY_DIALOG_FINISHED:
            done = true;
            break;
        case PSP_UTILITY_DIALOG_NONE:
            if (closing) done = true;
            break;
        default:
            if (state < 0) { result = state; done = true; }
            break;
        }
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceGuTerm();
    if (!running) return;
    /* The cancel button used in the dialog must not close the app on return. */
    SceCtrlData pad = {0};
    do {
        sceCtrlPeekBufferPositive(&pad, 1);
        sceKernelDelayThread(20000);
    } while (running && (pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)));
    if (result < 0 || config.base.result < 0) {
        disconnect();
        net_error("Blad Wi-Fi", result < 0 ? result : config.base.result);
        return;
    }
    if (config.base.result != 0) {
        disconnect();
        status = "Anulowano wybor sieci. Wybierz Uruchom.";
        return;
    }
    int state = 0;
    SceNetApctlInfo info;
    result = sceNetApctlGetState(&state);
    if (result < 0) {
        disconnect();
        net_error("Blad stanu Wi-Fi", result);
        return;
    }
    if (state != PSP_NET_APCTL_STATE_GOT_IP) {
        disconnect();
        snprintf(network_error, sizeof(network_error),
                 "Nie polaczono (stan %d). Wybierz Uruchom.", state);
        status = network_error;
        return;
    }
    result = sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info);
    if (result < 0) {
        disconnect();
        net_error("Blad odczytu IP", result);
        return;
    }
    snprintf(ip, sizeof(ip), "%.15s", info.ip);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = sceNetInetInetAddr(ip);
    server = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    if (server >= 0) sceNetInetSetsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (server < 0 || server >= FD_SETSIZE ||
        sceNetInetBind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        sceNetInetListen(server, 1) < 0) {
        disconnect();
        status = "Nie udalo sie otworzyc portu.";
        return;
    }
    snprintf(pin, sizeof(pin), "%06u",
             (unsigned)(sceKernelGetSystemTimeWide() % 1000000u));
    wrong_pins = 0;
    status = "Wpisz adres i PIN w aplikacji Android.";
}

#include "updater.h"

static void process_client(void)
{
    if (server < 0 || !ready(server, false, 0)) return;
    int fd = sceNetInetAccept(server, NULL, NULL);
    if (fd < 0) return;
    status = "Transfer... poczekaj.";
    draw();
    serve(fd);
    sceNetInetClose(fd);
    status = wrong_pins >= 5 ? "PIN zablokowany. Uruchom sesje ponownie." :
                              "Gotowe. Oczekiwanie na telefon.";
}

int main(void)
{
    int cb = sceKernelCreateThread("Callbacks", callbacks, 0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
    if (cb >= 0) sceKernelStartThread(cb, 0, NULL);
    vram = sceGeEdramGetAddr();
    sceDisplaySetMode(PSP_DISPLAY_MODE_LCD, 480, HEIGHT);
    pspDebugScreenInitEx((void *)((uintptr_t)vram | 0x40000000u),
                         PSP_DISPLAY_PIXEL_FORMAT_8888, 0);
    pspDebugScreenEnableBackColor(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    sceIoMkdir("ms0:/PSP/GAME/PSPLink", 0777);
    sceIoMkdir("ms0:/PSP/GAME/PSPLink/Transfer", 0777);
    draw();

    bool common = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON) >= 0;
    bool inet = common && sceUtilityLoadNetModule(PSP_NET_MODULE_INET) >= 0;
    bool pool = inet && sceNetInit(1024 * 1024, 0x30, 4096, 0x30, 4096) >= 0;
    bool sockets = pool && sceNetInetInit() >= 0;
    bool net = sockets && sceNetApctlInit(0x8000, 0x30) >= 0;
    if (!net) status = "Blad inicjalizacji sieci.";

    unsigned previous = 0;
    while (running) {
        SceCtrlData pad = {0};
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;
        if (pressed & PSP_CTRL_CIRCLE) break;
        if ((pressed & PSP_CTRL_LEFT) && category > 0) --category;
        if ((pressed & PSP_CTRL_RIGHT) && category < 3) ++category;
        if (pressed & PSP_CTRL_UP) row=0;
        if (pressed & PSP_CTRL_DOWN) row=1;
        if ((pressed & PSP_CTRL_CROSS) && net) {
            if (category==0 && row==1) { if(server>=0)disconnect();else connect_wifi(); }
            else if(category==0) status=server>=0?"Wpisz IP i PIN w aplikacji Android.":"Najpierw wybierz Połącz z Wi-Fi.";
            else if(category==1) status=row==0?"Na telefonie wybierz: Wgraj aplikację.":"Na telefonie wybierz: Wyślij plik.";
            else if(category==3) run_update(row);
        }
        process_client();
        draw();
    }
    updater_shutdown();
    if (net) {
        disconnect();
        sceNetApctlTerm();
    }
    if (sockets) sceNetInetTerm();
    if (pool) sceNetTerm();
    if (inet) sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    if (common) sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    sceKernelExitGame();
    return 0;
}
