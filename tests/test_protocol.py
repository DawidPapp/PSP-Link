"""Run the actual PSP protocol functions with POSIX filesystem/socket shims."""
from pathlib import Path
import socket
import subprocess
import tempfile
import struct
import os

project = Path(__file__).resolve().parents[1]
source = (project / 'psp/main.c').read_text()
functions = source[source.index('static bool ready('):source.index('static void disconnect(')]
shim = (project / 'tests/host_shim.h').read_text()

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    (root / 'Transfer').mkdir()
    (root / 'GAME').mkdir()
    (root / 'host.c').write_text(shim + functions +
        '\nint main(int argc,char **argv){if(argc!=2)return 2;serve(atoi(argv[1]));return 0;}\n')
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', str(root/'host.c'), '-o', str(root/'host')], check=True)

    def session(command, body=None, interrupt=False, fault=""):
        client, server = socket.socketpair()
        proc = subprocess.Popen([str(root/'host'), str(server.fileno())],
                                pass_fds=[server.fileno()], cwd=root,
                                env={**os.environ, "LINK_RENAME_FAULT": fault})
        server.close()
        client.settimeout(5)
        stream = client.makefile('rb')
        client.sendall(command.encode())
        response = stream.readline()
        if body is not None and response == b'GO\n':
            if body:
                client.sendall(body)
            if interrupt:
                client.shutdown(socket.SHUT_WR)
            response = stream.readline()
        if command.split()[1] == 'GET' and response.startswith(b'OK '):
            response += stream.read(int(response.split()[1]))
        if command.split()[1] == 'LIST' and response == b'OK\n':
            response += stream.read()
        stream.close()
        client.close()
        assert proc.wait(timeout=5) == 0
        return response

    def rejected(command, body=None, **kwargs):
        assert session(command, body, **kwargs).startswith(b'ERR'), command

    body = 'Zażółć gęślą jaźń!\n'.encode()
    assert session(f'123456 PUT test.txt {len(body)}\n', body) == b'OK\n'
    assert session('123456 GET test.txt 0\n') == f'OK {len(body)}\n'.encode() + body
    assert session('123456 PUT empty.txt 0\n', b'') == b'OK\n'
    assert b'test.txt\n' in session('123456 LIST - 0\n')
    rejected('123456 PUT test.txt 1\n', b'x')
    rejected('123456 PUT ../bad.txt 1\n')
    rejected('123456 PUT huge.txt 1048577\n')
    rejected('123456 PUT broken.txt 100\n', b'x', interrupt=True)
    assert not (root/'Transfer/broken.txt').exists()

    # A real, compiled PBP is copied without changing any bytes.
    pbp = (project/'psp/EBOOT.PBP').read_bytes()
    assert session(f'123456 APP Demo {len(pbp)}\n', pbp) == b'OK\n'
    assert (root/'GAME/Demo/EBOOT.PBP').read_bytes() == pbp
    rejected(f'123456 APP Demo {len(pbp)}\n')
    (root/'GAME/Existing').mkdir()
    rejected(f'123456 APP Existing {len(pbp)}\n')
    assert (root/'GAME/Demo/EBOOT.PBP').read_bytes() == pbp
    for name in ['../Escape', '/Escape', 'a/b', 'a\\b', '.', 'a:b', 'PSPLink', 'psplink', 'x'*41]:
        rejected(f'123456 APP {name} 40\n')
    rejected('000000 APP Denied 40\n')
    for size in ['0', '39', '33554433', '-1']:
        rejected(f'123456 APP BadSize {size}\n')
    bad = bytearray(40)
    rejected('123456 APP Invalid 40\n', bad)
    bad[:4] = b'\0PBP'
    for i in range(8, 40, 4):
        struct.pack_into('<I', bad, i, 40)
    struct.pack_into('<I', bad, 12, 41)
    rejected('123456 APP BadOffset 40\n', bad)
    rejected(f'123456 APP Broken {len(pbp)}\n', pbp[:40], interrupt=True)
    for name in ['Invalid', 'BadOffset', 'Broken', 'Denied', 'BadSize']:
        assert not (root/'GAME'/name).exists()
    assert not (root/'Transfer/.incoming').exists()
    # Never report success on a no-op rename, partial target, or failed rename.
    for fault in ['noop', 'error', 'truncate', 'badheader']:
        name = 'Fault_' + fault
        rejected(f'123456 APP {name} {len(pbp)}\n', pbp, fault=fault)
        assert not (root/'GAME'/name).exists()
    # Exact maximum is accepted and streamed in bounded buffers.
    payload = bytearray(32 * 1024 * 1024)
    payload[:40] = pbp[:40]
    assert session(f'123456 APP Maximum {len(payload)}\n', payload) == b'OK\n'
    assert (root/'GAME/Maximum/EBOOT.PBP').stat().st_size == len(payload)
    print('PASS: same-directory rename only; false success, truncation and bad header rejected; TXT regression; real PBP bytes; 32 MiB; bad header/offset/size/PIN/path; no overwrite; interrupted transfer cleanup.')
