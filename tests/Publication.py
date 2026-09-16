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
        self.manifest=dict(id='example.mod',environment='server',minimumApi=1,version='1.0.0',entry='Example.dll')
        (self.root/'mod-build.json').write_text(json.dumps(self.config));(self.root/'briefcase.mod.json').write_text(json.dumps(self.manifest))
        (self.root/'CMakeLists.txt').write_text('project(ExampleMod VERSION 1.0.0 LANGUAGES CXX)');(self.root/'dist').mkdir()
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
            with zipfile.ZipFile(archive,'w') as zipped:
                for name,data in entries.items():zipped.writestr('Briefcase/Mods/example.mod/'+name,data)
            checksum=archive.with_suffix('.zip.sha256');checksum.write_text(p.digest(archive)+'  '+archive.name+'\n')
            self.files.extend((archive,checksum))
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
    def test_public_repository_rejected(self):
        self.private=False
        with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
    def test_missing_linux_package_rejected(self):
        self.files[2].unlink()
        with self.assertRaises(OSError):self.publish()
        self.assertFalse(self.created)
    def test_checksum_rejected(self):
        self.files[1].write_text('bad')
        with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
    def test_upload_corruption_keeps_draft(self):
        self.corrupt=True
        with self.assertRaises(ValueError):self.publish()
        self.assertTrue(self.created);self.assertFalse(self.edited)
    def test_extra_file_rejected(self):
        with zipfile.ZipFile(self.files[0],'a') as zipped:zipped.writestr('unexpected.txt','no')
        self.files[1].write_text(p.digest(self.files[0])+'  '+self.files[0].name+'\n')
        with self.assertRaises(ValueError):self.publish()
        self.assertFalse(self.created)
if __name__=='__main__':unittest.main()
