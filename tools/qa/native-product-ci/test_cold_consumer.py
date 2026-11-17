# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import shutil
import unittest
import zipfile
import packed_registration as reg
import cold_schedule as cold
import cold_adapter
import gate
import artifact_transport

CONFIG=Path(__file__).resolve().parents[1]/'native-product-tests/suites.json'

class RegistrationTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.project=self.root/'project';self.app=self.root/'app';self.app.mkdir();(self.app/'browser').mkdir()
        self.entries={};lines=[]
        for mount in reg.MOUNTS:
            directory=self.project/'_dist/noraneko'/mount;directory.mkdir(parents=True)
            (directory/'index.js').write_text('asset:'+mount)
            namespace='noraneko-'+mount
            (directory/'jar.mn').write_text(f'noraneko.jar:\n% content {namespace} %nora-{mount}/ contentaccessible=yes\n nora-{mount}/index.js (index.js)\n')
            lines.append(f'content {namespace} noraneko/nora-{mount}/ contentaccessible=yes')
            self.entries[f'chrome/noraneko/nora-{mount}/index.js']=('asset:'+mount).encode()
        self.entries['chrome/chrome.manifest']='\n'.join(lines).encode();self.write()
    def tearDown(self):self.tmp.cleanup()
    def write(self):
        with zipfile.ZipFile(self.app/'omni.ja','w') as z:
            for k,v in self.entries.items():z.writestr(k,v)
        with zipfile.ZipFile(self.app/'browser/omni.ja','w'):pass
    def test_all_namespaces_and_generated_bytes(self):
        result=reg.verify(self.project,self.app)
        self.assertEqual(len(result['registrations']),11);self.assertEqual(len(result['assets']),11)
        self.assertEqual(result['native_execution'],'UNVERIFIED')
    def test_stale_asset_rejected(self):
        self.entries['chrome/noraneko/nora-startup/index.js']=b'stale';self.write()
        with self.assertRaises(ValueError):reg.verify(self.project,self.app)
    def test_unreachable_registration_rejected(self):
        self.entries['unreachable.manifest']=self.entries.pop('chrome/chrome.manifest');self.write()
        with self.assertRaises(ValueError):reg.verify(self.project,self.app)
    def test_remote_or_duplicate_registration_rejected(self):
        self.entries['chrome/chrome.manifest']+=b'\ncontent noraneko-startup https://example.test/ contentaccessible=yes';self.write()
        with self.assertRaises(ValueError):reg.verify(self.project,self.app)
    def test_generated_override_must_be_registered(self):
        jar=self.project/'_dist/noraneko/pages-aboutDialog/jar.mn'
        jar.write_text(jar.read_text().replace('noraneko.jar:\n','noraneko.jar:\n% override chrome://browser/content/aboutDialog.xhtml chrome://noraneko-pages-aboutDialog/content/index.js\n'))
        with self.assertRaises(ValueError):reg.verify(self.project,self.app)
        self.entries['chrome/chrome.manifest']+=b'\noverride chrome://browser/content/aboutDialog.xhtml chrome://noraneko-pages-aboutDialog/content/index.js'
        self.write();self.assertEqual(len(reg.verify(self.project,self.app)['overrides']),1)
    def test_generated_path_escape_rejected(self):
        (self.project/'_dist/noraneko/startup/jar.mn').write_text('noraneko.jar:\n% content noraneko-startup %nora-startup/ contentaccessible=yes\n ../escape (index.js)\n')
        with self.assertRaises(ValueError):reg.verify(self.project,self.app)

class ColdTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.support=self.root/'support';self.dumps=self.root/'dumps';self.dumps.mkdir();self.config=json.loads(CONFIG.read_text())
        for suite in self.config['suites']:
            if suite['id'] not in gate.PILOT_GROUPS:continue
            suite.pop('known_differences',None)
            path=suite['path'].rstrip('/')
            if suite['manifests']:path+='/'+suite['manifests'][0]
            manifest=self.support/(cold.PREFIX[suite['kind']]+path);manifest.parent.mkdir(parents=True,exist_ok=True)
            test=manifest.parent/('test_'+suite['id']+'.js');test.write_text('// fixture, not native execution')
            manifest.write_text('[DEFAULT]\nfirefox-appdir="browser"\n["'+test.name+'"]\n')
            (self.dumps/(suite['id']+'.dump.json')).write_text(json.dumps({'active_tests':[{'path':str(test)}]}))
        self.plan=cold.from_dumps(self.config,self.support,self.dumps,'a'*40,'b'*40)
    def tearDown(self):self.tmp.cleanup()
    def test_full_scope_native_dump_plan(self):
        self.assertEqual(len(self.plan['groups']),7)
        self.assertEqual(self.plan['excluded'],['p1-sessionstore-marionette'])
    def test_empty_foreign_or_disabled_dump_rejected(self):
        ident=gate.PILOT_GROUPS[0];p=self.dumps/(ident+'.dump.json');saved=p.read_text()
        variants=[{'active_tests':[]},{'active_tests':[{'path':'/outside/test.js'}]},json.loads(saved)]
        variants[-1]['active_tests'][0]['disabled']='condition'
        for data in variants:
            p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):cold.from_dumps(self.config,self.support,self.dumps,'a'*40,'b'*40)
    def test_manifest_missing_rejected(self):
        Path(self.plan['groups'][0]['manifest']).unlink()
        with self.assertRaises(ValueError):cold.manifests(self.config,self.support)
    def test_commands_keep_product_and_unfiltered_manifest(self):
        located=cold.manifests(self.config,self.support);mutable=self.root/'mutable'
        shutil.copytree(self.support/'mochitest',mutable)
        browser=cold.command(located['p0-tabs'],'/usr/bin/python3',self.support,self.root/'product',self.root/'out',{},dump=self.root/'dump.json',mochitest_root=mutable)
        self.assertIn('--dump-tests',browser);self.assertEqual(browser[browser.index('--appname')+1],str(self.root/'product/floorp'))
        unit=located['p1-sessionstore-unit']
        with self.assertRaises(ValueError):cold.command(unit,'/usr/bin/python3',self.support,self.root/'product',self.root/'out',{})
        wrapper=self.root/'wrapper.toml';wrapper.write_text('[DEFAULT]\nfloorp-appdir="browser"\n')
        unit={**unit,'appdir_wrapper':str(wrapper),'appdir_wrapper_sha256':cold.digest(wrapper)}
        argv=cold.command(unit,'/usr/bin/python3',self.support,self.root/'product',self.root/'out',{})
        self.assertEqual(argv[argv.index('--app-binary')+1],str(self.root/'product/floorp'))
        self.assertNotIn('--xpcshell',argv)
    def test_scope_cannot_be_replaced_with_canary_directory(self):
        self.config['suites'][0]['path']='browser/base/content/test/browser-performance/'
        with self.assertRaises(ValueError):cold.manifests(self.config,self.support)
    def test_real_log_bridge_and_negative_exit(self):
        evidence=self.root/'evidence';(evidence/'native').mkdir(parents=True);product=self.root/'product';product.mkdir()
        rows=[]
        for group in self.plan['groups']:
            ident=group['id'];test=group['resolved_test_ids'][0]
            events=[{'action':'suite_start'},{'action':'test_start','test':test}]
            if group['kind']=='xpcshell':
                argv=[str(product/'floorp'),'--xpcshell','-g',str(product),'-a',str(product/'browser')]
                events.append({'action':'log','level':'INFO','message':test+' | full command: '+repr(argv)})
            events += [{'action':'test_status','test':test,'subtest':'fixture','status':'PASS'},{'action':'test_end','test':test,'status':'PASS'},{'action':'suite_end'}]
            (evidence/'native'/(ident+'.jsonl')).write_text('\n'.join(json.dumps(e) for e in events));rows.append({'id':ident,'exit_code':0})
        run={'schema':'floorp-cold-native-execution-v1','plan_sha256':hashlib.sha256(json.dumps(self.plan,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'product_unchanged':True,'support_unchanged':True,'results':rows,'finished':True,'interrupted':False,'owned_cleanup_complete':True,'all_processes_finished':True}
        _,argv,verdict=cold_adapter.finalize(self.plan,run,product,evidence)
        self.assertEqual(verdict['executed'],7);self.assertEqual(len(argv),2)
        run['results'][0]['exit_code']=1
        with self.assertRaises(ValueError):cold_adapter.finalize(self.plan,run,product,evidence)
        run['results'][0]['exit_code']=0;run['product_unchanged']=False
        with self.assertRaises(ValueError):cold_adapter.finalize(self.plan,run,product,evidence)

class QualificationTests(unittest.TestCase):
    def test_different_runtime_requires_explicit_isolated_qualification(self):
        lock={'schemaVersion':1,'source':{'repository':artifact_transport.preflight.RUNTIME,'commit':'a'*40}}
        pin={'runtime_sha':'b'*40}
        with self.assertRaises(ValueError):artifact_transport.verify_pr_lock(lock,pin)
        pin['qualification']={'schema':'floorp-debug-qualification-v1','scope':'isolated-browser-xpcshell-qa','production_runtime_sha':'a'*40,'debug_runtime_sha':'b'*40,'production_equivalence':False}
        self.assertEqual(artifact_transport.verify_pr_lock(lock,pin)['mode'],'separate-debug-qualification')
        self.assertEqual(lock['source']['commit'],'a'*40)
        pin['qualification']['production_equivalence']=True
        with self.assertRaises(ValueError):artifact_transport.verify_pr_lock(lock,pin)

if __name__=='__main__':unittest.main()
