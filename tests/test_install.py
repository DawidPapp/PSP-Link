from pathlib import Path
import subprocess,tempfile
project=Path(__file__).resolve().parents[1]
source=(project/'psp/main.c').read_text()
header=source[source.index('static bool pbp_header('):source.index('static void upload(')]
u=(project/'psp/updater.h').read_text()
verify=u[u.index('static bool verify_update('):u.index('static void run_update(')]
shim=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include "update_format.h"
#define GAME "GAME/"
#define PSP_O_RDONLY O_RDONLY
#define PSP_O_WRONLY O_WRONLY
#define PSP_O_CREAT O_CREAT
#define PSP_O_TRUNC O_TRUNC
#define sceIoOpen open
#define sceIoClose close
#define sceIoWrite write
#define sceIoRead read
#define sceIoMkdir mkdir
#define sceIoRmdir rmdir
#define sceIoRemove unlink
#define FIO_S_ISREG S_ISREG
#define SceUID int
#define SceIoStat struct stat
#define sceIoGetstat stat
static int rename_fault;
static int sceIoRename(const char*a,const char*b){return rename_fault?0:rename(a,b);}
typedef SHA256_CTX mbedtls_sha256_context;
static void mbedtls_sha256_init(mbedtls_sha256_context*c){memset(c,0,sizeof(*c));}
static int mbedtls_sha256_starts_ret(mbedtls_sha256_context*c,int type){(void)type;return SHA256_Init(c)?0:-1;}
static int mbedtls_sha256_update_ret(mbedtls_sha256_context*c,const void*b,size_t n){return SHA256_Update(c,b,n)?0:-1;}
static int mbedtls_sha256_finish_ret(mbedtls_sha256_context*c,unsigned char*b){return SHA256_Final(b,c)?0:-1;}
static void mbedtls_sha256_free(mbedtls_sha256_context*c){(void)c;}
static bool running=true,update_ready=true;
static UpdateInfo pending_update;
static char pending_repo[101]="test/repo",update_status[112],update_title[64],update_detail[96];
static const char*status;
static void draw(void){}
static void update_message(const char*s){snprintf(update_status,sizeof(update_status),"%s",s);status=update_status;}
typedef struct {char*memory;unsigned used,limit;SceUID file;} Download;
static const char *fixture;
static int failure;
static bool https_get(const char*url,Download*d){
 assert(!strcmp(url,"https://github.com/test/repo/releases/download/v0.5.1/EBOOT.PBP"));
 FILE*f=fopen(fixture,"rb");assert(f);unsigned char b[4096];size_t n;
 while((n=fread(b,1,sizeof(b),f))){if(failure==2)b[0]^=1;assert(n<=d->limit-d->used);assert(write(d->file,b,n)==(int)n);d->used+=n;if(failure==1){fclose(f);return false;}}
 fclose(f);return true;
}
'''
tests=r'''
int main(int argc,char**argv){
 assert(argc==2);fixture=argv[1];mkdir("GAME",0700);
 FILE*f=fopen(fixture,"rb");assert(f);mbedtls_sha256_context c;SHA256_Init(&c);unsigned char b[4096];size_t n;while((n=fread(b,1,sizeof(b),f))){SHA256_Update(&c,b,n);pending_update.size+=n;}fclose(f);SHA256_Final(pending_update.sha,&c);strcpy(pending_update.version,"0.5.1");
 for(failure=1;failure<=2;++failure){install_update();assert(access("GAME/PSPLink-0.5.1",F_OK)!=0);}
 failure=0;rename_fault=1;install_update();assert(access("GAME/PSPLink-0.5.1",F_OK)!=0);
 rename_fault=0;install_update();assert(!update_ready);assert(verify_update("GAME/PSPLink-0.5.1/EBOOT.PBP",&pending_update));assert(access("GAME/PSPLink-0.5.1/LINK.TMP",F_OK)!=0);
 install_update();assert(verify_update("GAME/PSPLink-0.5.1/EBOOT.PBP",&pending_update));
 puts("PASS: update installation, interrupted download, bad hash, no-op rename, no overwrite, readback.");
}
'''
with tempfile.TemporaryDirectory() as temp:
 root=Path(temp);(root/'test.c').write_text(shim+header+verify+tests)
 subprocess.run(['cc','-Wall','-Wextra','-Wno-deprecated-declarations','-Wno-unused-function','-I',str(project/'psp'),str(root/'test.c'),'-lcrypto','-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test'),str(project/'psp/EBOOT.PBP')],cwd=root,check=True)
