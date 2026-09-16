"""Validate both native packages and publish them together in a private repository."""
import argparse, hashlib, json, re, struct, subprocess, tempfile, zipfile
from pathlib import Path

def require(value, message):
    if not value: raise ValueError(message)
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def command(*args): return subprocess.check_output(args, text=True).strip()
def source(root, version, commit):
    require(re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version), 'Invalid version')
    require(re.fullmatch('[a-f0-9]{40}', commit), 'Invalid source commit')
    config=json.loads((root/'mod-build.json').read_text())
    require((root/'VERSION').read_text().strip()==version, 'Release/source version mismatch')
    manifest=json.loads((root/'briefcase.mod.json.in').read_text().replace('@MOD_VERSION@',version))
    require(manifest['version']==version, 'Manifest must use VERSION')
    require(command('git','-C',str(root),'rev-parse','HEAD')==commit, 'Release/source commit mismatch')
    try:
        tag_commit=command('git','-C',str(root),'rev-parse','--verify','--quiet','refs/tags/v'+version+'^{commit}')
    except subprocess.CalledProcessError as error:
        require(error.returncode==1,'Cannot inspect release tag')
    else:require(tag_commit==commit,'Existing tag points to another commit')
    require(manifest['environment']=='server' and manifest['minimumApi']==1 and
            re.fullmatch('[a-z0-9.-]+',manifest['id']) and re.fullmatch(r'[A-Za-z0-9_.-]+\.dll',manifest['entry']), 'Invalid server manifest')
    require(re.fullmatch('[A-Za-z0-9_.-]+',config['repository']) and
            re.fullmatch('[a-f0-9]{64}',config['sdkSha256']), 'SDK checksum must be pinned before publication')
    return config,manifest
def archive(root, config, manifest, version, platform):
    path=root/'dist'/f"{config['repository']}-{platform}-x64-{version}.zip"
    checksum=path.with_suffix('.zip.sha256')
    require(checksum.read_text().strip()==digest(path)+'  '+path.name, 'Archive checksum mismatch')
    expected_manifest=dict(manifest)
    if platform=='linux':expected_manifest['entry']=manifest['entry'].removesuffix('.dll')+'.so'
    prefix='Briefcase/Mods/'+manifest['id']+'/'
    names={prefix+name for name in (expected_manifest['entry'],'briefcase.mod.json','Data/config.json','Licenses/nlohmann-json.txt')}
    if platform=='linux':names|={prefix+'Licenses/GCC-runtime.txt',prefix+'Licenses/GPL-3.txt'}
    with zipfile.ZipFile(path) as zipped:
        actual=zipped.namelist()
        require(len(actual)==len(names) and set(actual)==names,'Unexpected mod archive content')
        for item in zipped.infolist():
            mode=item.external_attr>>16
            require(not item.is_dir() and mode&0o170000 in (0,0o100000) and not mode&0o7000 and
                    not item.flag_bits&1 and item.file_size<=64*1024*1024,'Invalid archive entry')
        require(json.loads(zipped.read(prefix+'briefcase.mod.json'))==expected_manifest,'Packaged manifest mismatch')
        json.loads(zipped.read(prefix+'Data/config.json'))
        binary=zipped.read(prefix+expected_manifest['entry'])
        if platform=='linux':
            require(len(binary)>=20 and binary[:6]==b'\x7fELF\x02\x01' and binary[18:20]==b'\x3e\0','Expected Linux x64 ELF')
        else:
            require(len(binary)>=64 and binary[:2]==b'MZ','Expected Windows DLL')
            offset=struct.unpack_from('<I',binary,0x3c)[0]
            require(offset+24<=len(binary) and binary[offset:offset+6]==b'PE\0\0\x64\x86','Expected Windows x64 PE')
    return path,checksum
def publish(root, repository, version, commit, draft=False):
    config,manifest=source(root,version,commit)
    require(re.fullmatch('[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',repository) and repository.split('/')[1]==config['repository'],'Wrong destination repository')
    files=[file for platform in ('windows','linux') for file in archive(root,config,manifest,version,platform)]
    info=json.loads(command('gh','api','repos/'+repository))
    require(info['private'] is True and info['full_name']==repository,'Publication requires the intended private repository')
    tag='v'+version
    notes=(f"Native server mod {version} for BriefcaseNative {config['sdkVersion']}.\n\n"
           'Separate Windows x64 DLL and Linux x64 SO packages, each with a SHA-256 checksum. '
           'Stop the server before installation and preserve existing Data/config.json. '
           'Linux packages contain only native code and data; no Python runtime is needed.\n\n'
           'Build and lifecycle tests run separately on both platforms. Connected-player testing under Linux remains required.\n')
    with tempfile.TemporaryDirectory() as directory:
        body=Path(directory)/'notes.md';body.write_text(notes,encoding='utf-8')
        command('gh','release','create',tag,*(str(file) for file in files),'--repo',repository,'--target',commit,
                '--title',config['repository']+' '+version,'--notes-file',str(body),'--draft')
    api=command('gh','release','view',tag,'--repo',repository,'--json','apiUrl','--jq','.apiUrl')
    require(re.fullmatch('https://api.github.com/repos/'+re.escape(repository)+'/releases/[0-9]+',api),'Unexpected draft URL')
    release=json.loads(command('gh','api',api))
    require(release['draft'] and release['tag_name']==tag and release['target_commitish']==commit and len(release['assets'])==4,'Unexpected release identity')
    for file in files:
        items=[item for item in release['assets'] if item['name']==file.name]
        require(len(items)==1 and items[0]['state']=='uploaded' and items[0]['size']==file.stat().st_size and
                items[0]['digest']=='sha256:'+digest(file),'Uploaded asset verification failed')
    require(json.loads(command('gh','api','repos/'+repository))['private'] is True,'Repository visibility changed; draft retained')
    if not draft:command('gh','release','edit',tag,'--repo',repository,'--draft=false','--latest')
    print('Verified private mod release:',repository,tag)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--version',required=True);parser.add_argument('--commit',required=True)
    parser.add_argument('--repository');parser.add_argument('--draft',action='store_true');parser.add_argument('--source-only',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    if args.source_only:source(root,args.version,args.commit)
    else:publish(root,args.repository,args.version,args.commit,args.draft)
