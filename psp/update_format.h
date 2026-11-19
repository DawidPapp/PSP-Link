#ifndef UPDATE_FORMAT_H
#define UPDATE_FORMAT_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#define CURRENT_VERSION "0.5.0"
#define DEFAULT_REPOSITORY "DawidPapp/PSP-Link"
#define UPDATE_LIMIT (32u*1024u*1024u)
typedef struct { char version[16]; unsigned size; unsigned char sha[32]; } UpdateInfo;
static bool repo_valid(const char *s) {
 size_t n=strlen(s); if(n<3||n>100)return false;
 unsigned slash=0; const char *component=s;
 for(const char *p=s;;++p) {
  char c=*p;
  if(c=='/'||!c){if(p==component||*component=='.'||p[-1]=='.')return false; if(!c)break; component=p+1; ++slash;}
  else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
 }
 return slash==1;
}
static bool version_number(const char *s,unsigned *v) {
 unsigned a,b,c; int end=0;
 if(sscanf(s,"%u.%u.%u%n",&a,&b,&c,&end)!=3||s[end]||a>999||b>999||c>999)return false;
 char canonical[16]; snprintf(canonical,sizeof(canonical),"%u.%u.%u",a,b,c);
 if(strcmp(canonical,s))return false;
 *v=a*1000000+b*1000+c; return true;
}
static int unhex(char c) {if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; return -1;}
static bool manifest_parse(const char *s,UpdateInfo *out) {
 char version[16],hash[65],extra; unsigned size;
 if(sscanf(s,"PSPLINK1 %15s %u %64s %c",version,&size,hash,&extra)!=3||strlen(hash)!=64||size<40||size>UPDATE_LIMIT)return false;
 unsigned value;if(!version_number(version,&value))return false;
 for(int i=0;i<32;++i){int a=unhex(hash[2*i]),b=unhex(hash[2*i+1]);if(a<0||b<0)return false;out->sha[i]=(a<<4)|b;}
 strcpy(out->version,version);out->size=size;return true;
}
#endif
