/* HTTPS-only GitHub Releases updater. Trust comes from verified TLS; SHA-256
   checks the artifact against the manifest fetched from the configured repo. */
#include <curl/curl.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/sha256.h>
#include "ui/ca.h"
static UpdateInfo pending_update;
static bool update_ready;
static char pending_repo[101], update_status[112];
static mbedtls_x509_crt authorities;
static bool authorities_ready, curl_ready;
static uint32_t progress_tick;
typedef struct {char *memory; unsigned used,limit; SceUID file;} Download;
static void update_message(const char *message){snprintf(update_status,sizeof(update_status),"%s",message);status=update_status;draw();}
static bool read_repo(char *repo){
 strcpy(repo,DEFAULT_REPOSITORY);
 return repo_valid(repo);
}
static CURLcode ssl_context(CURL *curl,void *ctx,void *user){
 (void)curl;(void)user;
 mbedtls_ssl_conf_ca_chain((mbedtls_ssl_config*)ctx,&authorities,NULL);
 return CURLE_OK;
}
static size_t download_write(char *data,size_t size,size_t count,void *user){
 Download *d=user;if(size && count>SIZE_MAX/size)return 0;size_t n=size*count;
 if(n>d->limit-d->used)return 0;
 if(d->memory)memcpy(d->memory+d->used,data,n);
 else {size_t off=0;while(off<n){int wrote=sceIoWrite(d->file,data+off,n-off);if(wrote<=0)return 0;off+=wrote;}}
 d->used+=n;return n;
}
static int download_progress(void *user,curl_off_t total,curl_off_t now,curl_off_t ut,curl_off_t un){
 (void)user;(void)ut;(void)un;
 SceCtrlData pad={0};sceCtrlPeekBufferPositive(&pad,1);
 if(!running || (pad.Buttons&PSP_CTRL_CIRCLE))return 1;
 uint32_t tick=sceKernelGetSystemTimeLow();
 if(tick-progress_tick>150000){progress_tick=tick;
  if(total>0)snprintf(update_status,sizeof(update_status),"Pobieranie: %u%%   O: anuluj",(unsigned)(now*100/total));
  else snprintf(update_status,sizeof(update_status),"HTTPS: %u KiB   O: anuluj",(unsigned)(now/1024));
  status=update_status;draw();}
 return 0;
}
static bool https_get(const char *url,Download *data){
 if(!curl_ready){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){update_message("Nie można uruchomić HTTPS.");return false;}curl_ready=true;}
 const curl_version_info_data *version=curl_version_info(CURLVERSION_NOW);
 if(!version->ssl_version || !strstr(version->ssl_version,"mbedTLS")){update_message("Nieobsługiwany moduł TLS w tej kompilacji.");return false;}
 if(!authorities_ready){mbedtls_x509_crt_init(&authorities);int result=mbedtls_x509_crt_parse(&authorities,trusted_ca,sizeof(trusted_ca));
  if(result<0){mbedtls_x509_crt_free(&authorities);update_message("Nie można wczytać certyfikatów HTTPS.");return false;}authorities_ready=true;}
 CURL *handle=curl_easy_init();if(!handle){update_message("Brak pamięci dla HTTPS.");return false;}
 CURLcode result=CURLE_OK;
 #define OPT(key,value) do {if(result==CURLE_OK)result=curl_easy_setopt(handle,key,value);}while(0)
 OPT(CURLOPT_URL,url);OPT(CURLOPT_USERAGENT,"PSP-Link/" CURRENT_VERSION);
 OPT(CURLOPT_PROTOCOLS,(long)CURLPROTO_HTTPS);OPT(CURLOPT_REDIR_PROTOCOLS,(long)CURLPROTO_HTTPS);
 OPT(CURLOPT_FOLLOWLOCATION,1L);OPT(CURLOPT_MAXREDIRS,5L);OPT(CURLOPT_FAILONERROR,1L);
 OPT(CURLOPT_SSL_VERIFYPEER,1L);OPT(CURLOPT_SSL_VERIFYHOST,2L);OPT(CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);
 OPT(CURLOPT_CAINFO,(char*)NULL);OPT(CURLOPT_CAPATH,(char*)NULL);OPT(CURLOPT_SSL_CTX_FUNCTION,ssl_context);
 OPT(CURLOPT_CONNECTTIMEOUT,30L);OPT(CURLOPT_TIMEOUT,600L);OPT(CURLOPT_LOW_SPEED_LIMIT,128L);OPT(CURLOPT_LOW_SPEED_TIME,45L);
 OPT(CURLOPT_NOSIGNAL,1L);OPT(CURLOPT_HTTP_VERSION,(long)CURL_HTTP_VERSION_1_1);
 OPT(CURLOPT_WRITEFUNCTION,download_write);OPT(CURLOPT_WRITEDATA,data);
 OPT(CURLOPT_NOPROGRESS,0L);OPT(CURLOPT_XFERINFOFUNCTION,download_progress);
 #undef OPT
 progress_tick=0;draw();if(result==CURLE_OK)result=curl_easy_perform(handle);
 long response=0;curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&response);curl_easy_cleanup(handle);
 if(result!=CURLE_OK || response!=200){
  if(result==CURLE_ABORTED_BY_CALLBACK)update_message("Anulowano pobieranie.");
  else {snprintf(update_status,sizeof(update_status),"HTTPS: błąd %d, HTTP %ld",(int)result,response);status=update_status;draw();}
  return false;}
 return true;
}
static bool verify_update(const char *path,const UpdateInfo *expected){
 SceIoStat st;if(sceIoGetstat(path,&st)<0||!FIO_S_ISREG(st.st_mode)||st.st_size!=expected->size)return false;
 SceUID f=sceIoOpen(path,PSP_O_RDONLY,0);if(f<0)return false;
 mbedtls_sha256_context hash;mbedtls_sha256_init(&hash);bool ok=mbedtls_sha256_starts_ret(&hash,0)==0;
 unsigned char buffer[4096],digest[32];unsigned count=0;
 while(ok&&count<expected->size&&running){unsigned chunk=expected->size-count;if(chunk>sizeof(buffer))chunk=sizeof(buffer);
  int n=sceIoRead(f,buffer,chunk);if(n<=0){ok=false;break;}
  if(count==0&&(n<40||!pbp_header(buffer,expected->size))){ok=false;break;}
  ok=mbedtls_sha256_update_ret(&hash,buffer,n)==0;count+=n;}
 if(sceIoClose(f)<0)ok=false;
 ok=ok&&count==expected->size&&mbedtls_sha256_finish_ret(&hash,digest)==0;
 mbedtls_sha256_free(&hash);return ok&&!memcmp(digest,expected->sha,32);
}
static void install_update(void){
 char dir[100],temp[120],final[120],url[260];
 snprintf(dir,sizeof(dir),GAME "PSPLink-%s",pending_update.version);
 SceIoStat st;if(sceIoGetstat(dir,&st)>=0){update_message("Folder tej wersji już istnieje. Nie nadpisuję.");return;}
 snprintf(temp,sizeof(temp),"%s/LINK.TMP",dir);snprintf(final,sizeof(final),"%s/EBOOT.PBP",dir);
 snprintf(url,sizeof(url),"https://github.com/%s/releases/download/v%s/EBOOT.PBP",pending_repo,pending_update.version);
 if(sceIoMkdir(dir,0777)<0){update_message("Nie można utworzyć folderu aktualizacji.");return;}
 SceUID f=sceIoOpen(temp,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0666);
 if(f<0){sceIoRmdir(dir);update_message("Nie można zapisać aktualizacji.");return;}
 Download data={NULL,0,pending_update.size,f};bool ok=https_get(url,&data);int closed=sceIoClose(f);
 if(ok){update_message("Sprawdzanie rozmiaru i SHA-256…");ok=closed>=0&&data.used==pending_update.size&&verify_update(temp,&pending_update);
  if(!ok)update_message("Błąd rozmiaru, PBP lub SHA-256. Zapis odrzucony.");}
 bool renamed=false;if(ok){renamed=sceIoRename(temp,final)>=0;ok=renamed&&verify_update(final,&pending_update);
  if(!ok)update_message("Nie udało się potwierdzić docelowego EBOOT.PBP.");}
 if(!ok){sceIoRemove(temp);if(renamed)sceIoRemove(final);sceIoRmdir(dir);return;}
 update_ready=false;strcpy(update_title,"Sprawdź aktualizacje");strcpy(update_detail,"Nowa wersja zapisana obok obecnej");
 snprintf(update_status,sizeof(update_status),"Zapisano %s. Wyjdź do Gra / Memory Stick.",pending_update.version);status=update_status;draw();
}
static void run_update(int action){
 char repo[101];
 if(action==0&&!read_repo(repo)){update_ready=false;strcpy(update_title,"Sprawdź aktualizacje");strcpy(update_detail,"Niepoprawna konfiguracja kompilacji");update_message("Błąd wbudowanego adresu repozytorium");return;}
 if(server<0){connect_wifi();if(server<0)return;}
 if(action==1){char body[16385];Download data={body,0,sizeof(body)-1,-1};if(https_get("https://api.github.com/",&data))update_message("GitHub HTTPS działa. Certyfikat sprawdzony.");}
 else if(update_ready&&!strcmp(repo,pending_repo))install_update();
 else {
  update_ready=false;strcpy(update_title,"Sprawdź aktualizacje");
  char url[240],body[513];snprintf(url,sizeof(url),"https://github.com/%s/releases/latest/download/psplink-update.txt",repo);
  Download data={body,0,sizeof(body)-1,-1};if(!https_get(url,&data))return;body[data.used]=0;
  if(memchr(body,0,data.used)||!manifest_parse(body,&pending_update)){update_message("Niepoprawny opis aktualizacji.");return;}
  unsigned latest,current;version_number(pending_update.version,&latest);version_number(CURRENT_VERSION,&current);
  if(latest<=current){update_message("Masz aktualną lub nowszą wersję PSP Link.");return;}
  strcpy(pending_repo,repo);update_ready=true;
  snprintf(update_title,sizeof(update_title),"Pobierz wersję %s",pending_update.version);
  snprintf(update_detail,sizeof(update_detail),"%u KiB · X: pobierz do nowego folderu",(pending_update.size+1023)/1024);
  update_message("Znaleziono aktualizację. X potwierdza pobranie.");
 }
 /* Consume held cancel/confirm before returning to the main menu. */
 SceCtrlData pad={0};do{sceCtrlPeekBufferPositive(&pad,1);sceKernelDelayThread(20000);}while(running&&(pad.Buttons&(PSP_CTRL_CROSS|PSP_CTRL_CIRCLE)));
}
static void updater_shutdown(void){if(authorities_ready)mbedtls_x509_crt_free(&authorities);if(curl_ready)curl_global_cleanup();}
