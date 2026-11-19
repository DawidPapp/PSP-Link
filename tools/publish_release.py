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
        return json.load(response)
try:
    existing=request(api+'/releases/tags/v'+version)
except urllib.error.HTTPError as e:
    if e.code!=404: raise
else:
    if existing.get('draft'): raise SystemExit('An unfinished draft exists; inspect it before retrying.')
    print('Version already published; leaving existing release unchanged:',existing['html_url'])
    raise SystemExit(0)
release=request(api+'/releases','POST',{
    'tag_name':'v'+version,'target_commitish':os.environ['GITHUB_SHA'],
    'name':'PSP Link '+version,'draft':True,'prerelease':False,
    'body':'Wydanie testowe aktualizatora PSP Link. Odtworzony interfejs i bezpośrednie pobieranie GitHub HTTPS.\n\nPobierz EBOOT.PBP. Plik psplink-update.txt jest przeznaczony dla aktualizatora. Kompilacja oraz testy komputerowe są wykonywane przez GitHub Actions; działanie na PSP-3004 OFW 6.20 wymaga testu na konsoli.\n\nZainstaluj w nowym folderze PSP/GAME, zachowując działającą wersję.'})
upload=release['upload_url'].split('{',1)[0]
for name in ['EBOOT.PBP','psplink-update.txt']:
    request(upload+'?name='+name,'POST',(root/'release'/name).read_bytes(),'application/octet-stream')
result=request(api+'/releases/'+str(release['id']),'PATCH',{'draft':False,'make_latest':'true'})
print('Published:',result['html_url'])
