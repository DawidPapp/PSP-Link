"""Publish the current build from GitHub Actions after successful tests.
Creates a draft first; makes it public only after both assets are uploaded.
"""
import json, os, urllib.request, urllib.error
from pathlib import Path
root=Path(__file__).resolve().parents[1]
version=(root/'release/psplink-update.txt').read_text().splitlines()[1]
repo=os.environ['GITHUB_REPOSITORY']
api=os.environ.get('GITHUB_API_URL','https://api.github.com')+'/repos/'+repo
headers={'Authorization':'Bearer '+os.environ['GH_TOKEN'],'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'PSP-Link-release'}
def request(url,method='GET',body=None,content_type='application/json'):
    if isinstance(body,dict): body=json.dumps(body).encode()
    req=urllib.request.Request(url,data=body,method=method,headers={**headers,'Content-Type':content_type})
    with urllib.request.urlopen(req,timeout=120) as response:
        data=response.read()
        return json.loads(data) if data else None
def retire_renumbered_release():
    # One-time, explicitly requested renumbering. Never touch a later 0.5.1.
    if version != '0.5.0': return
    expected = 'ceca2eade4eea129dece69f77a00e99f2d496763'
    try:
        old = request(api+'/releases/tags/v0.5.1')
    except urllib.error.HTTPError as e:
        if e.code == 404: return
        raise
    tag = request(api+'/git/ref/tags/v0.5.1')
    if tag['object']['sha'] != expected or tag['object']['type'] != 'commit':
        raise SystemExit('Old tag changed: refusing to remove a different release.')
    request(api+'/releases/'+str(old['id']),'DELETE')
    request(api+'/git/refs/tags/v0.5.1','DELETE')
    print('Retired old test release v0.5.1 after publishing its replacement v0.5.0.')

try:
    existing=request(api+'/releases/tags/v'+version)
except urllib.error.HTTPError as e:
    if e.code!=404: raise
else:
    if existing.get('draft'): raise SystemExit('An unfinished draft exists; inspect it before retrying.')
    print('Version already published; leaving existing release unchanged:',existing['html_url'])
    retire_renumbered_release()
    raise SystemExit(0)
release=request(api+'/releases','POST',{
    'tag_name':'v'+version,'target_commitish':os.environ['GITHUB_SHA'],
    'name':'PSP Link '+version,'draft':True,'prerelease':False,
    'body':f'PSP Link {version} z fabrycznym repozytorium DawidPapp/PSP-Link. Nie wymaga repozytorium.txt. Numer wcześniejszego wydania testowego 0.5.1 skorygowano na 0.5.0.\n\nPobierz EBOOT.PBP. Plik psplink-update.txt jest przeznaczony dla aktualizatora. Kompilacja oraz testy komputerowe są wykonywane przez GitHub Actions; działanie na PSP-3004 OFW 6.20 wymaga testu na konsoli.\n\nZainstaluj w nowym folderze PSP/GAME, zachowując działającą wersję.'})
upload=release['upload_url'].split('{',1)[0]
for name in ['EBOOT.PBP','psplink-update.txt']:
    request(upload+'?name='+name,'POST',(root/'release'/name).read_bytes(),'application/octet-stream')
result=request(api+'/releases/'+str(release['id']),'PATCH',{'draft':False,'make_latest':'true'})
print('Published:',result['html_url'])
retire_renumbered_release()
