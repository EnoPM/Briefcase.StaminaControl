"""No-network contracts for the two-platform private release publisher."""
import importlib.util,json,struct,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('publisher',Path(__file__).resolve().parents[1]/'scripts/publish.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
class Publication(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.config=dict(repository='ExampleMod',sdkVersion='0.5.0',sdkSha256='a'*64)
        self.manifest=dict(id='example.mod',environment='server',minimumApi=1,version='1.0.0',entry='Example.dll',
                           update=dict(provider='github-releases',repository='EnoPM/ExampleMod'))
        (self.root/'mod-build.json').write_text(json.dumps(self.config));(self.root/'briefcase.mod.json.in').write_text(json.dumps(dict(self.manifest,version='@MOD_VERSION@')))
        (self.root/'VERSION').write_text('1.0.0\n');(self.root/'dist').mkdir()
        self.commit='b'*40;self.repository='Example/ExampleMod';self.files=[]
        for platform in ('windows','linux'):
            manifest=dict(self.manifest)
            if platform=='linux':manifest['entry']='Example.so'
            binary=bytearray(128)
            if platform=='windows':binary[:2]=b'MZ';struct.pack_into('<I',binary,0x3c,64);binary[64:70]=b'PE\0\0\x64\x86'
            else:binary[:6]=b'\x7fELF\x02\x01';binary[18:20]=b'\x3e\0'
            entries={manifest['entry']:bytes(binary),'briefcase.mod.json':json.dumps(manifest).encode(),'Data/config.json':b'{}','Licenses/nlohmann-json.txt':b'license'}
            if platform=='linux':entries.update({'Licenses/GCC-runtime.txt':b'license','Licenses/GPL-3.txt':b'license'})
            archive=self.root/'dist'/f'ExampleMod-{platform}-x64-1.0.0.zip'
            prefix='Briefcase/Mods/example.mod/'
            rows=[dict(path=prefix+name,bytes=len(data),sha256=__import__('hashlib').sha256(data).hexdigest(),mode=0o644,
                       **({'preserve':True} if name=='Data/config.json' else {})) for name,data in entries.items()]
            package=dict(updateSchema=1,kind='briefcase-mod',platform=platform+'-x64',modId='example.mod',version='1.0.0',
                         repository='EnoPM/ExampleMod',files=rows)
            with zipfile.ZipFile(archive,'w') as zipped:
                for name,data in entries.items():zipped.writestr(prefix+name,data)
                zipped.writestr('ModPackage.json',json.dumps(package))
            self.files.append(archive)
        self.private=True;self.created=False;self.edited=False;self.corrupt=False
    def cli(self,*args):
        if args[0]=='git':return self.commit
        if args==('gh','api','repos/'+self.repository):return json.dumps(dict(private=self.private,full_name=self.repository))
        if args[:3]==('gh','release','create'):self.created=True;return ''
        if args[:3]==('gh','release','view'):return 'https://api.github.com/repos/'+self.repository+'/releases/1'
        if args[:2]==('gh','api'):
            return json.dumps(dict(draft=True,tag_name='v1.0.0',target_commitish=self.commit,assets=[dict(name=f.name,state='uploaded',size=f.stat().st_size,digest='sha256:'+('0'*64 if self.corrupt else p.digest(f))) for f in self.files]))
        if args[:3]==('gh','release','edit'):self.edited=True;return ''
        raise AssertionError(args)
    def publish(self):
        with patch.object(p,'command',side_effect=self.cli):p.publish(self.root,self.repository,'1.0.0',self.commit)
    def test_both_platforms_published(self):
        self.publish();self.assertTrue(self.created and self.edited)
    def test_version_file_alone_drives_generated_manifest(self):
        (self.root/'VERSION').write_text('1.2.3\n')
        with patch.object(p,'command',side_effect=self.cli):
            _,manifest=p.source(self.root,'1.2.3',self.commit)
        self.assertEqual(manifest['version'],'1.2.3')
    def test_invalid_or_mismatched_version_rejected(self):
        for value in ('v1.0.0','01.0.0','1.0.0-beta','1.0.0\ninjected','2.0.0'):
            (self.root/'VERSION').write_text(value)
            with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
    def test_public_repository_rejected(self):
        self.private=False
        with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
    def test_missing_linux_package_rejected(self):
        self.files[1].unlink()
        with self.assertRaises(OSError):self.publish()
        self.assertFalse(self.created)
    def test_upload_corruption_keeps_draft(self):
        self.corrupt=True
        with self.assertRaises(ValueError):self.publish()
        self.assertTrue(self.created);self.assertFalse(self.edited)
    def test_extra_file_rejected(self):
        with zipfile.ZipFile(self.files[0],'a') as zipped:zipped.writestr('unexpected.txt','no')
        with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
if __name__=='__main__':unittest.main()
