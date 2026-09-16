"""Build, test and package this server mod with a separately supplied public SDK."""
import argparse,hashlib,json,re,shutil,stat,subprocess,tempfile,zipfile
from pathlib import Path
def run(*args):subprocess.run([str(arg) for arg in args],check=True)
def main():
 p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);args=p.parse_args()
 root=Path(__file__).resolve().parents[1];sdk=args.sdk.resolve();config=json.loads((root/'mod-build.json').read_text())
 inventory=json.loads((sdk/'SDK.json').read_text())
 assert inventory['version']==config['sdkVersion'] and inventory['abiVersion']==1
 for row in inventory['files']:
  path=(sdk/row['path']).resolve();assert path.is_relative_to(sdk)
  assert hashlib.sha256(path.read_bytes()).hexdigest()==row['sha256']
 build=root/'build-linux'
 run('cmake','-S',root,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_COMPILER=clang-19','-DCMAKE_CXX_COMPILER=clang++-19','-DCMAKE_PREFIX_PATH='+str(sdk),'-DBriefcaseNativeSDK_DIR='+str(sdk/'cmake'))
 run('cmake','--build',build,'--parallel','4');run('ctest','--test-dir',build,'--output-on-failure')
 manifest=json.loads((build/'briefcase.mod.json').read_text())
 assert manifest['environment']=='server' and manifest['minimumApi']==1 and re.fullmatch('[a-z0-9.-]+',manifest['id']) and re.fullmatch('[A-Za-z0-9_.-]+[.]so',manifest['entry'])
 assert re.fullmatch('[A-Za-z0-9_.-]+',config['repository']) and re.fullmatch('[0-9]+[.][0-9]+[.][0-9]+',manifest['version'])
 output=root/'dist';output.mkdir(exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='linux-package-',dir=build) as directory:
  stage=Path(directory);run('cmake','--install',build,'--prefix',stage)
  mod=stage/'Briefcase/Mods'/manifest['id'];licenses=mod/'Licenses';licenses.mkdir()
  shutil.copyfile(sdk/'Licenses/nlohmann-json.txt',licenses/'nlohmann-json.txt')
  shutil.copyfile('/usr/share/doc/gcc-13-base/copyright',licenses/'GCC-runtime.txt')
  shutil.copyfile('/usr/share/common-licenses/GPL-3',licenses/'GPL-3.txt')
  run('strip','--strip-unneeded',mod/manifest['entry'])
  expected={manifest['entry'],'briefcase.mod.json','Data/config.json','Licenses/nlohmann-json.txt','Licenses/GCC-runtime.txt','Licenses/GPL-3.txt'}
  assert {file.relative_to(mod).as_posix() for file in mod.rglob('*') if file.is_file()}==expected
  assert len([file for file in stage.rglob('*') if file.is_file()])==len(expected)
  archive=output/(config['repository']+'-linux-x64-'+manifest['version']+'.zip')
  with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as zipped:
   for file in sorted(stage.rglob('*')):
    if not file.is_file():continue
    info=zipfile.ZipInfo(file.relative_to(stage).as_posix(),(2020,1,1,0,0,0));info.external_attr=(stat.S_IFREG|0o644)<<16;info.create_system=3;info.compress_type=zipfile.ZIP_DEFLATED
    zipped.writestr(info,file.read_bytes())
  archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
  print('Verified Linux mod package:',archive)
if __name__=='__main__':main()
