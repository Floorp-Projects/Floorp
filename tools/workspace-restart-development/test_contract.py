import copy
import json
from pathlib import Path
import socket
import struct
import threading
import tempfile
import unittest
import run
from protocol import Marionette,MAX_FRAME
from session import decode,validate_saved

def frame(value):
    data=json.dumps(value,ensure_ascii=False).encode();return str(len(data)).encode()+b':'+data

class Protocol(unittest.TestCase):
    def client(self):
        a,b=socket.socketpair();a.setblocking(False)
        c=Marionette.__new__(Marionette);c.sock=a;c.pump=lambda:None;c.trace=lambda data:None;c.buffer=bytearray();c.index=0
        self.addCleanup(a.close);self.addCleanup(b.close);return c,b
    def test_fragmented_utf8_correlated_reply(self):
        c,b=self.client();packet=frame([1,0,None,{'value':'日本語'}])
        def server():
            b.recv(65536)
            for part in [packet[:2],packet[2:8],packet[8:]]:b.sendall(part)
        t=threading.Thread(target=server);t.start();self.assertEqual(c.command('test')['value'],'日本語');t.join()
    def test_wrong_reply_identity_closes_no_retry(self):
        c,b=self.client()
        def server():b.recv(65536);b.sendall(frame([1,1,None,{}]))
        t=threading.Thread(target=server);t.start()
        with self.assertRaisesRegex(ValueError,'identity mismatch'):c.command('test')
        self.assertEqual(c.sock.fileno(),-1);t.join()
    def test_boolean_and_float_reply_ids_close_without_retry(self):
        for ident in [False,0.0]:
            c,b=self.client()
            def server():b.recv(65536);b.sendall(frame([1,ident,None,{}]))
            t=threading.Thread(target=server);t.start()
            with self.subTest(ident=ident),self.assertRaisesRegex(ValueError,'identity mismatch'):c.command('test')
            self.assertEqual(c.sock.fileno(),-1);t.join()
    def test_oversized_or_malformed_prefix_reject(self):
        for data in [str(MAX_FRAME+1).encode()+b':',b'bad:',b'0000000000:']:
            c,b=self.client();c.buffer.extend(data)
            with self.assertRaises(ValueError):c.read(float('inf'))

class Session(unittest.TestCase):
    def encode(self,state):
        data=json.dumps(state).encode();length=len(data);extra=bytearray()
        if length>=15:
            n=length-15
            while n>=255:extra.append(255);n-=255
            extra.append(n)
        return b'mozLz40\0'+struct.pack('<I',length)+bytes([min(length,15)<<4])+extra+data
    def test_clean_file_and_workspace_binding(self):
        expected={'case':'workspace-exit','retainedURL':'about:robots#unique','b':'workspace-b'}
        state={'windows':[],'_closedWindows':[{'tabs':[{'entries':[{'url':expected['retainedURL']}],'floorpWorkspaceId':'workspace-b'}]}]}
        self.assertTrue(validate_saved(decode(self.encode(state)),expected))
        state['_closedWindows'][0]['tabs'][0]['floorpWorkspaceId']='wrong'
        with self.assertRaises(ValueError):validate_saved(decode(self.encode(state)),expected)
    def test_corrupt_or_bomb_file_reject(self):
        for data in [b'bad',b'mozLz40\0'+struct.pack('<I',5000000),b'mozLz40\0'+struct.pack('<I',4)+b'\x00\x00\x00']:
            with self.assertRaises(ValueError):decode(data)

class Ownership(unittest.TestCase):
    def test_shared_feles_session_is_not_browser_tree(self):
        table={10:{'start':'root','ppid':5,'session':5,'state':'S'},11:{'start':'vite','ppid':5,'session':5,'state':'S'},12:{'start':'content','ppid':10,'session':5,'state':'S'}}
        tracked={10:'root'};root={'pid':10,'session':5}
        self.assertEqual(run.capture_browser(table,tracked,root),{10,12});self.assertNotIn(11,tracked)
    def test_zombie_generation_not_disappeared(self):
        self.assertEqual(run.present({10:{'start':'old','state':'Z'}},{10:'old'}),{10})
    def test_owned_session_keeps_reparented_child_and_zombie_after_root_exit(self):
        table={11:{'start':'server','ppid':1,'session':5,'state':'S'},12:{'start':'child','ppid':1,'session':10,'state':'S'},13:{'start':'zombie','ppid':1,'session':10,'state':'Z'}}
        tracked={10:'root'};root={'pid':10,'session':10,'start':'root'}
        self.assertEqual(run.capture_browser(table,tracked,root),{12})
        self.assertEqual(run.present(table,tracked),{12,13});self.assertNotIn(11,tracked)
        table[10]={'start':'new-root','session':10,'ppid':1,'state':'S'}
        with self.assertRaisesRegex(ValueError,'generation reused'):run.capture_browser(table,tracked,root)

class Isolation(unittest.TestCase):
    def test_control_preserves_original_session_and_workspace_prefs(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile=Path(tmp);saved=profile/'sessionstore.jsonlz4';saved.write_bytes(b'original clean bytes')
            prefs=profile/'prefs.js';prefs.write_text('user_pref("floorp.workspaces.v4.store", "original");\nuser_pref("nora.tests.filter.9", "stale");\n')
            before=run.digest(saved);a,values=run.write_boot_control(profile,'normal',1,42)
            self.assertEqual(values['nora.tests.filter.count'],'1');self.assertEqual(json.loads(values['nora.tests.filter']),[run.ISOLATION_TARGET])
            self.assertIn('floorp.workspaces.v4.store',prefs.read_text());self.assertNotIn('stale',prefs.read_text());self.assertEqual(before,run.digest(saved))
            b,_=run.write_boot_control(profile,'normal',2,42);self.assertNotEqual(a['runId'],b['runId']);self.assertEqual(before,run.digest(saved))
    def test_empty_foreign_expired_or_broad_control_reject(self):
        control={'schemaVersion':1,'runId':'boot1','expiresAtMs':2000,'filter':[run.ISOLATION_TARGET]}
        run.validate_boot_control(control,1000)
        for patch in [{'filter':[]},{'filter':['browser-features/chrome/test/other.test.ts']},{'runId':''},{'expiresAtMs':999},{'expiresAtMs':500000}]:
            with self.subTest(patch=patch),self.assertRaises(ValueError):run.validate_boot_control({**control,**patch},1000)
        state={'runId':'boot1','status':'done','results':[],'discoveredFiles':[],'aborted':False,'startupTest':'loaded','completedAtMs':1500}
        run.validate_isolation(state,control)
        for patch in [{'runId':'old'},{'status':'running'},{'results':[{'ok':True}]},{'discoveredFiles':['other.test.ts']},{'aborted':True},{'error':'original error'},{'completedAtMs':2000}]:
            with self.subTest(patch=patch),self.assertRaises(ValueError):run.validate_isolation({**state,**patch},control)

class Contract(unittest.TestCase):
    def receipt(self):
        rows=[];files={}
        for i,(case,name) in enumerate(run.CASES.items()):
            profile={'realpath':'/owned/profile','device':1,'inode':123+i,'markerSha256':f'{i+1:064x}'};nonce=f'{i+1:032x}';boots=[]
            for n,p in enumerate([10+i*20,20+i*20],1):
                control={'schemaVersion':1,'runId':f'{case}-boot{n}','expiresAtMs':2000,'filter':[run.ISOLATION_TARGET]}
                isolation={'runId':control['runId'],'status':'done','results':[],'discoveredFiles':[],'aborted':False,'startupTest':'loaded','completedAtMs':1000}
                saved={'evidenceFile':case+f'-{n}-clean-session.jsonlz4','bytes':20,'sha256':f'{p:064x}'}
                boots.append({'root':{'pid':p,'start':str(p),'session':p},'profileIdentity':profile,'native':{'profile':'/owned/profile','pid':p,'buildID':'20261007125927','version':'157.0.1','restoreCompleted':True,'bootstrapIsolation':isolation},'control':control,'normalExit':True,'exitCode':0,'oldTreeGone':True,'portGone':True,'binarySha256':'b'*64,'overlayBuildID2':'same-overlay','exitedBrowserIdentities':{str(p):str(p)},'cleanSessionFile':saved})
                files[saved['evidenceFile']]={'bytes':saved['bytes'],'sha256':saved['sha256']}
                files[case+f'-boot{n}-control.json']={'bytes':len((json.dumps(run.control_evidence(control),indent=2)+'\n').encode()),'sha256':run.json_digest(run.control_evidence(control))}
                files[case+f'-boot{n}-protocol.log']={'bytes':1,'sha256':'a'*64}
            row={'case':case,'name':name,'nonce':nonce,'profileIdentity':profile,'index':i+1,'status':'PASS_DEVELOPMENT_ONLY','boots':boots,'oldBrowserTreeGoneBeforeSecondBoot':True,'oldPortGoneBeforeSecondBoot':True,'cleanSessionSaved':True,'syntheticProfileRemoved':True,'firstError':None,'cleanSessionBeforeSecondBootSha256':boots[0]['cleanSessionFile']['sha256'],'consumedOriginalCleanSessionSha256':boots[0]['cleanSessionFile']['sha256']}
            for field in ['prepared','verified']:
                row[field]={'ok':True,'phase':field,'case':case,'nonce':nonce,'state':{'identity':copy.deepcopy(boots[0 if field=='prepared' else 1]['native'])}}
                body=(json.dumps(row[field],indent=2)+'\n').encode();files[case+'-'+field+'.json']={'bytes':len(body),'sha256':run.json_digest(row[field])}
            rows.append(row)
        for name in ['feles.log','workspace-exit-boot1.log','normal-boot2.log','workspace-exit-boot2.log']:files[name]={'bytes':1,'sha256':'a'*64}
        return {'schema':'qa3-workspace-restart-development-v1','scope':'ORDINARY_DEVELOPMENT_E2E','status':'PASS_DEVELOPMENT_ONLY','floorpHead':'c'*40,'run':42,'attempt':1,'runtimeSource':'d'*40,'runtimeArtifact':'locked.tar.xz','runtimeVersion':'157.0.1','runtimeBuildID':'20261007125927','sourceHashes':{p:'e'*64 for p in run.SOURCE_PATHS},'nativeProductQualification':'UNVERIFIED','compileProfileQualification':'UNVERIFIED','publicationAuthorized':False,'nativeForcedCleanup':False,'expectedCaseCount':2,'cases':rows,'evidenceFiles':files,'ownedCleanupComplete':True,'originalError':None,'cleanupError':None}
    def test_exact_two_real_pairs(self):run.validate_receipt(self.receipt())
    def test_missing_duplicate_skip_forced_stale_or_changed_boot_reject(self):
        for change in ['missing','duplicate','skip','forced','samepid','profile','build','child','firsterror','cleanup','schema','attempt','source','qualified','nativeforced','saved','phase','inventory']:
            r=copy.deepcopy(self.receipt());row=r['cases'][0];b=row['boots'][1]
            if change=='missing':r['cases'].pop()
            elif change=='duplicate':r['cases'].append(row)
            elif change=='skip':row['status']='SKIPPED'
            elif change=='forced':b['normalExit']=False
            elif change=='samepid':b['root']=row['boots'][0]['root']
            elif change=='profile':b['profileIdentity']={**b['profileIdentity'],'inode':999}
            elif change=='build':b['overlayBuildID2']='different'
            elif change=='child':row['oldBrowserTreeGoneBeforeSecondBoot']=False
            elif change=='firsterror':r['originalError']='first failure'
            elif change=='cleanup':r['cleanupError']='cleanup failure'
            elif change=='schema':r['schema']='other'
            elif change=='attempt':r['attempt']=2
            elif change=='source':r['sourceHashes'].pop(next(iter(r['sourceHashes'])))
            elif change=='qualified':r['nativeProductQualification']='PASS'
            elif change=='nativeforced':r['nativeForcedCleanup']=True
            elif change=='saved':row['consumedOriginalCleanSessionSha256']='f'*64
            elif change=='phase':row['verified']['nonce']='foreign'
            else:r['evidenceFiles'].pop('normal-1-clean-session.jsonlz4')
            with self.subTest(change=change),self.assertRaises(ValueError):run.validate_receipt(r)
    def test_cross_case_native_generations_cannot_be_relabeled(self):
        r=self.receipt();foreign=r['cases'][0]['boots'][0];second=r['cases'][1]['boots'][0]
        second['root']=copy.deepcopy(foreign['root']);second['native']['pid']=foreign['root']['pid'];second['exitedBrowserIdentities']=foreign['exitedBrowserIdentities']
        r['cases'][1]['prepared']['state']['identity']=copy.deepcopy(second['native'])
        with self.assertRaisesRegex(ValueError,'Four distinct'):run.validate_receipt(r)

if __name__=='__main__':unittest.main()
