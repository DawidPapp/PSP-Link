#include <math.h>
#include "font.h"
static uint32_t rgb(int r,int g,int b){return 0xff000000u|(unsigned)b<<16|(unsigned)g<<8|(unsigned)r;}
static void dot(int x,int y,uint32_t c,int a){if(x<0||x>=480||y<0||y>=272||a<=0)return;if(a>255)a=255;unsigned o=pixels[y*STRIDE+x],n=255-a; pixels[y*STRIDE+x]=rgb(((o&255)*n+(c&255)*a)/255,(((o>>8)&255)*n+((c>>8)&255)*a)/255,(((o>>16)&255)*n+((c>>16)&255)*a)/255);}
static unsigned codepoint(const char **s){const unsigned char *p=(const unsigned char*)*s;unsigned c=*p++;if((c&224)==192&&*p){c=((c&31)<<6)|(*p++&63);}else if((c&240)==224&&p[0]&&p[1]){c=((c&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}*s=(const char*)p;return c;}
static const Glyph *glyph(unsigned c,int size){const Glyph *g=size==20?glyph20:size==14?glyph14:glyph11;for(unsigned i=0;i<GLYPHS;++i)if(g[i].code==c)return g+i;return g+'?'-32;}
static int width(const char*s,int size){int w=0;while(*s)w+=glyph(codepoint(&s),size)->advance;return w;}
static void text_at(int x,int y,int size,uint32_t c,int a,const char*s){const unsigned char *f=size==20?font20:size==14?font14:font11;while(*s&&x<460){const Glyph*g=glyph(codepoint(&s),size);for(int j=0;j<g->h;++j)for(int i=0;i<g->w;++i)dot(x+i,y+j,c,f[g->offset+j*g->w+i]*a/255);x+=g->advance;}}
static void line(float x,float y,float u,float v,int a){int left=floorf(fminf(x,u)-2),right=ceilf(fmaxf(x,u)+2),top=floorf(fminf(y,v)-2),bottom=ceilf(fmaxf(y,v)+2);float dx=u-x,dy=v-y,len=dx*dx+dy*dy;for(int j=top;j<=bottom;++j)for(int i=left;i<=right;++i){float t=len?((i-x)*dx+(j-y)*dy)/len:0;t=fmaxf(0,fminf(1,t));float xx=i-x-t*dx,yy=j-y-t*dy;float cover=fmaxf(0,fminf(1,1.3f-sqrtf(xx*xx+yy*yy)));dot(i,j,rgb(243,249,255),a*cover);}}
static void arc(float x,float y,float r,float begin,float end,int a){float ox=x+r*cosf(begin),oy=y+r*sinf(begin);for(int i=1;i<=24;++i){float t=begin+(end-begin)*i/24,px=x+r*cosf(t),py=y+r*sinf(t);line(ox,oy,px,py,a);ox=px;oy=py;}}
static void icon(int kind,int x,int y,int a){
#define L(i,j,k,m) line(x+(i)*.8f,y+(j)*.8f,x+(k)*.8f,y+(m)*.8f,a)
 switch(kind){
 case 0:arc(x+12.8f,y+15.2f,11.2f,3.84f,5.585f,a);arc(x+12.8f,y+16.8f,7.2f,3.84f,5.585f,a);arc(x+12.8f,y+20,1.2f,0,6.283f,a);break;
 case 1:L(3,10,3,7);L(3,7,13,7);L(13,7,17,11);L(17,11,29,11);L(29,11,29,26);L(29,26,3,26);L(3,26,3,10);break;
 case 2:L(4,10,27,10);L(22,5,27,10);L(22,15,27,10);L(28,23,5,23);L(10,18,5,23);L(10,28,5,23);break;
 case 3:arc(x+12.8f,y+12.8f,8.8f,.87f,5.76f,a);L(25,6,26,14);L(19,13,26,14);break;
 default:L(16,5,16,22);L(10,11,16,5);L(22,11,16,5);L(5,21,5,27);L(5,27,27,27);L(27,27,27,21);break;}
#undef L
}
static void item(int x,int y,int kind,const char*title,const char*detail,bool selected){if(selected)for(int j=0;j<51;++j)for(int i=0;i<432;++i)dot(24+i,y+j,rgb(74,182,255),37-i*32/432);icon(kind,x+8,y+13,255);text_at(x+53,y+4,14,rgb(243,249,255),255,title);text_at(x+53,y+27,11,rgb(178,207,236),255,detail);}
static void draw(void){static int previous=-1;static float shift,alpha[4]={1,.48f,.48f,.48f};if(previous!=category){shift=previous<0?0:(category>previous?22:-22);previous=category;}shift*=.78f;uint32_t*front=vram+(frame++&1u)*STRIDE*HEIGHT;
#ifdef UI_HOST_TEST
pixels=front;
#else
pixels=(volatile uint32_t*)((uintptr_t)front|0x40000000u);
#endif
 for(int y=0;y<HEIGHT;++y){float t=y/(float)HEIGHT;int r,g,b;if(t<.58f){t/=.58f;r=8+8*t;g=21+31*t;b=47+43*t;}else{t=(t-.58f)/.42f;r=16+8*t;g=52+44*t;b=90+45*t;}for(int x=0;x<480;++x)pixels[y*STRIDE+x]=rgb(r,g,b);}
 float time=(sceKernelGetSystemTimeLow()%120000000u)/12000000.f;for(int k=0;k<3;++k)for(int x=0;x<480;++x){float u=x/480.f;int y=HEIGHT*.66f+HEIGHT*.055f*sinf(u*4.4f+time+k*.3f)+HEIGHT*.018f*sinf(u*8-time*.6f);for(int d=0;d<12+k*10;++d)dot(x,y+d,rgb(109,216,255),k?18:32);}
 uint32_t white=rgb(243,249,255),soft=rgb(178,207,236);text_at(24,8,20,white,255,"PSP Link");char info[100];snprintf(info,sizeof(info),"%s · " CURRENT_VERSION,server>=0?ip:"Rozłączono");text_at(456-width(info,11),17,11,soft,255,info);
 const char*tabs[]={"Połączenie","Memory Stick","Transfery","Aktualizacje"};for(int i=0;i<4;++i){int cx=78+i*108;alpha[i]+=((i==category?1:.48f)-alpha[i])*.25f;icon(i,cx-13,47,alpha[i]*255);text_at(cx-width(tabs[i],11)/2,81,11,white,alpha[i]*255,tabs[i]);}
 int x=24+(int)shift;
 if(category==0){if(server>=0)snprintf(info,sizeof(info),"IP %s   PIN %s",ip,pin);else strcpy(info,"Adres IP i PIN pojawią się po połączeniu");item(x,110,0,"Dane połączenia",info,row==0);item(x,166,3,server>=0?"Rozłącz":"Połącz z Wi-Fi",server>=0?"Zakończ sesję z telefonem":"Wybierz sieć w systemowym oknie PSP",row==1);}
 else if(category==1){item(x,110,1,"Wgraj aplikację","EBOOT.PBP do PSP/GAME · do 32 MiB",row==0);item(x,166,4,"Wyślij plik","Pliki .txt do 1 MiB · folder Transfer",row==1);}
 else if(category==2){snprintf(info,sizeof(info),"Odebrano: %u",received);item(x,110,2,info,"Pliki zapisane na Memory Stick",false);snprintf(info,sizeof(info),"Wysłano: %u",sent);item(x,166,2,info,"Pliki pobrane na telefon",false);}
 else {item(x,110,3,update_title,update_detail,row==0);item(x,166,0,"Test HTTPS z GitHub","Bez repozytorium · bez pobierania aktualizacji",row==1);}
 text_at(24,232,11,soft,255,status);text_at(24,253,11,soft,200,"Krzyżak: kategorie i opcje    X Wybierz    O Wyjdź");sceDisplaySetFrameBuf(front,STRIDE,PSP_DISPLAY_PIXEL_FORMAT_8888,PSP_DISPLAY_SETBUF_NEXTFRAME);sceDisplayWaitVblankStart();}
