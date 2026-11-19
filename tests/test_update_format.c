#include <assert.h>
#include "../psp/update_format.h"
int main(void){
 assert(!strcmp(DEFAULT_REPOSITORY,"DawidPapp/PSP-Link") && repo_valid(DEFAULT_REPOSITORY));
 const char *bad[]={"","owner","a/b/c","../repo","a/../b","a/repo?x","a/repo#x","a/repo%20","a/","/b","a/b\n","a/..",".a/b"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!repo_valid(bad[i]));
 assert(repo_valid("User-1/PSP.Link"));
 unsigned n;assert(version_number("0.5.1",&n)&&n==5001);
 const char *versions[]={"-1.0.0","1.2","1.2.3a","01.2.3","1000.0.0","1.2.3/evil","+1.2.3"};
 for(unsigned i=0;i<sizeof(versions)/sizeof(*versions);++i)assert(!version_number(versions[i],&n));
 UpdateInfo info;char s[256];
 const char *hash="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
 snprintf(s,sizeof(s),"PSPLINK1\n0.5.1\n40\n%s\n",hash);assert(manifest_parse(s,&info)&&info.size==40&&info.sha[0]==1);
 snprintf(s,sizeof(s),"PSPLINK1\n0.5.1\n33554433\n%s\n",hash);assert(!manifest_parse(s,&info));
 snprintf(s,sizeof(s),"PSPLINK1\n0.5.1\n39\n%s\n",hash);assert(!manifest_parse(s,&info));
 snprintf(s,sizeof(s),"PSPLINK1\n0.5.1\n40\n%s\nextra",hash);assert(!manifest_parse(s,&info));
 assert(!manifest_parse("PSPLINK1 0.5.1 40 abc",&info));
 puts("PASS: repository and manifest validation, path injection, size, version and digest formats.");
}
