"""Download and verify the pinned public SDK for Linux builds."""
import hashlib,json,re,stat,tempfile,urllib.request,zipfile
from pathlib import Path

def fetch(root):
    config=json.loads((root/'mod-build.json').read_text())
    assert re.fullmatch('[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',config['sdkRepository'])
    assert re.fullmatch(r'\d+\.\d+\.\d+',config['sdkVersion']) and re.fullmatch('[a-f0-9]{64}',config['sdkSha256'])
    cache=root/'.sdk';cache.mkdir(exist_ok=True);work=Path(tempfile.mkdtemp(prefix='linux-',dir=cache))
    name=f"BriefcaseNative-SDK-{config['sdkVersion']}.zip"
    url=f"https://github.com/{config['sdkRepository']}/releases/download/v{config['sdkVersion']}/{name}"
    with urllib.request.urlopen(url,timeout=60) as response:
        data=response.read(16*1024*1024+1)
    assert len(data)<=16*1024*1024 and hashlib.sha256(data).hexdigest()==config['sdkSha256'],'SDK checksum mismatch'
    archive=work/name;archive.write_bytes(data);sdk=work/'sdk';sdk.mkdir()
    with zipfile.ZipFile(archive) as zipped:
        seen=set();total=0
        for item in zipped.infolist():
            name=item.filename;mode=item.external_attr>>16
            assert re.fullmatch('[A-Za-z0-9_./-]+',name) and all(part not in ('','..','.') for part in name.split('/'))
            assert name not in seen and not item.is_dir() and stat.S_IFMT(mode) in (0,stat.S_IFREG);seen.add(name)
            total+=item.file_size;assert total<=64*1024*1024
        zipped.extractall(sdk)
    manifest=json.loads((sdk/'SDK.json').read_text());assert manifest['version']==config['sdkVersion'] and manifest['abiVersion']==1
    expected={'SDK.json'}
    for row in manifest['files']:
        name=row['path'];path=(sdk/name).resolve();assert path.is_relative_to(sdk) and name not in expected;expected.add(name)
        assert path.stat().st_size==row['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==row['sha256']
    assert seen==expected
    return sdk
if __name__=='__main__':print(fetch(Path(__file__).resolve().parents[1]))
