"""State-machine regression with mocked PSP utility; not a hardware Wi-Fi test."""
from pathlib import Path
import subprocess
import tempfile

project=Path(__file__).resolve().parents[1]
source=(project/'psp/main.c').read_text()
disconnect=source[source.index('static void disconnect('):source.index('/* A separate GU frame')]
connect=source[source.index('static void net_error('):source.index('#include "updater.h"')]
tests=r'''
static void reset(void) {
    init_result=dialog_result=info_result=bind_result=0;
    final_state=4; socket_result=6; opened=shutdowns=disconnected=closed=0;
    finished=false; server=-1;
}
int main(void) {
    reset(); connect_wifi();
    assert(server==6 && opened==1 && shutdowns==1 && polls>1500);
    assert(!strcmp(ip,"192.168.1.7") && !strcmp(pin,"123456"));
    reset(); dialog_result=1; connect_wifi();
    assert(server==-1 && opened==0 && shutdowns==1 && strstr(status,"Anulowano"));
    /* Retry immediately after cancellation. */
    dialog_result=0; connect_wifi(); assert(server==6 && opened==1);
    reset(); init_result=-7; connect_wifi();
    assert(opened==0 && strstr(status,"FFFFFFF9"));
    reset(); dialog_result=-8; connect_wifi();
    assert(opened==0 && shutdowns==1 && strstr(status,"FFFFFFF8"));
    reset(); final_state=2; connect_wifi();
    assert(opened==0 && strstr(status,"stan 2"));
    reset(); info_result=-9; connect_wifi();
    assert(opened==0 && strstr(status,"Blad odczytu IP"));
    reset(); bind_result=-1; connect_wifi();
    assert(server==-1 && closed==1 && strstr(status,"portu"));
    puts("PASS: system dialog success, >20-second flow, cancel/retry, init/result/IP/bind failures; server starts only after IP.");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    (root/'test.c').write_text((project/'tests/wifi_shim.h').read_text()+disconnect+connect+tests)
    subprocess.run(['cc','-Wall','-Wextra','-Werror',str(root/'test.c'),'-o',str(root/'test')],check=True)
    subprocess.run([str(root/'test')],check=True)
