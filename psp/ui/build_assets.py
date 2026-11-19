from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json
root=Path(__file__).parent
chars=''.join(chr(i) for i in range(32,127))+'ąćęłńóśźżĄĆĘŁŃÓŚŹŻ·…'
out=['typedef struct {unsigned code,offset;unsigned char w,h,advance;} Glyph;']
for size in [11,14,20]:
 f=ImageFont.truetype('/usr/share/fonts/truetype/open-sans/OpenSans-Light.ttf',size*3);data=[];glyph=[]
 for c in chars:
  a=max(1,round(f.getlength(c)/3));w=a+3;h=size+6
  im=Image.new('L',(w*3,h*3));ImageDraw.Draw(im).text((0,size*3),c,font=f,fill=255,anchor='ls');im=im.resize((w,h),Image.Resampling.LANCZOS)
  glyph.append((ord(c),len(data),w,h,a));data.extend(im.tobytes())
 out+=['static const unsigned char font%d[] = {%s};'%(size,','.join(map(str,data))), 'static const Glyph glyph%d[] = {%s};'%(size,','.join('{'+','.join(map(str,g))+'}' for g in glyph))]
out+=['#define GLYPHS %d'%len(chars), 'static const char font_license[] __attribute__((used)) = '+json.dumps('Open Sans Copyright 2010-2011 Google Corporation.\n'+Path('/usr/share/common-licenses/Apache-2.0').read_text())+';']
(root/'font.h').write_text('\n'.join(out))
pem=(root/'cacert.pem').read_bytes()
(root/'ca.h').write_text('static const unsigned char trusted_ca[] = {'+','.join(map(str,pem))+',0};\n')
