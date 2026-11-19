"""Prepare GitHub Release assets from a compiled PSP Link EBOOT.PBP."""
from pathlib import Path
import hashlib,re,struct,sys,shutil
project=Path(__file__).resolve().parents[1]
version=re.search(r'#define CURRENT_VERSION "([0-9.]+)"',(project/'psp/update_format.h').read_text())[1]
source=Path(sys.argv[1]) if len(sys.argv)>1 else project/'psp/EBOOT.PBP'
data=source.read_bytes()
if len(data)<40 or len(data)>32*1024*1024: raise SystemExit('Invalid PBP size')
magic,_,*offsets=struct.unpack('<10I',data[:40])
if magic!=0x50425000 or offsets!=sorted(offsets) or offsets[0]<40 or offsets[-1]>len(data):raise SystemExit('Invalid PBP header')
if ('PSP Link '+version).encode()+b'\0' not in data[offsets[0]:offsets[1]]:raise SystemExit('PBP title/version does not match source')
out=project/'release';out.mkdir(exist_ok=True)
shutil.copyfile(source,out/'EBOOT.PBP')
(out/'psplink-update.txt').write_text(f'PSPLINK1\n{version}\n{len(data)}\n{hashlib.sha256(data).hexdigest()}\n')
print(f'Attach EBOOT.PBP and psplink-update.txt to public GitHub Release v{version}.')
