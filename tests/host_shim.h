#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROOT "Transfer/"
#define GAME "GAME/"
#define PART ROOT ".incoming"
enum {LIMIT=1024*1024, APP_LIMIT=32*1024*1024};
typedef int SceUID;
typedef struct timeval SceNetInetTimeval;
typedef struct { mode_t st_mode; off_t st_size; } SceIoStat;
typedef struct {char d_name[256]; SceIoStat d_stat;} SceIoDirent;
static DIR *directory;
static bool running=true;
static unsigned received, sent, wrong_pins;
static char pin[]="123456";
#define PSP_O_RDONLY O_RDONLY
#define PSP_O_WRONLY O_WRONLY
#define PSP_O_CREAT O_CREAT
#define PSP_O_TRUNC O_TRUNC
#define sceIoOpen open
#define sceIoRead read
#define sceIoWrite write
#define sceIoClose close
static int sceIoRename(const char *from, const char *to) {
 const char *a=strrchr(from,'/'), *b=strrchr(to,'/');
 /* Reject cross-directory moves even when the host filesystem supports them. */
 if(!a || !b || (a-from)!=(b-to) || strncmp(from,to,a-from)) return -1;
 const char *mode=getenv("LINK_RENAME_FAULT");
 if(mode && !strcmp(mode,"noop")) return 0;
 if(mode && !strcmp(mode,"error")) return -1;
 int result=rename(from,to);
 if(result==0 && mode && !strcmp(mode,"truncate")) truncate(to,1);
 if(result==0 && mode && !strcmp(mode,"badheader")) {
   int fd=open(to,O_WRONLY); if(fd>=0) { write(fd,"BAD!",4); close(fd); }
 }
 return result;
}
#define sceIoRemove unlink
#define sceIoMkdir mkdir
#define sceIoRmdir rmdir
#define sceNetInetSelect select
#define sceNetInetSend send
#define sceNetInetRecv recv
#define FIO_S_ISREG S_ISREG
static int sceIoGetstat(const char *p,SceIoStat *out) {
 struct stat s;
 if(stat(p,&s)<0) return -1;
 out->st_mode=s.st_mode; out->st_size=s.st_size; return 0;
}
static int sceIoDopen(const char *p) {directory=opendir(p);return directory?1:-1;}
static int sceIoDread(int fd,SceIoDirent *out) {
 (void)fd; struct dirent *d=readdir(directory); if(!d)return 0;
 snprintf(out->d_name,sizeof(out->d_name),"%s",d->d_name);
 char p[512]; snprintf(p,sizeof(p),ROOT "%s",d->d_name);
 return sceIoGetstat(p,&out->d_stat)<0?-1:1;
}
static int sceIoDclose(int fd) {(void)fd;return closedir(directory);}
static uint32_t sceKernelGetSystemTimeLow(void) {
 struct timeval t;gettimeofday(&t,0);return (uint32_t)((uint64_t)t.tv_sec*1000000+t.tv_usec);
}
