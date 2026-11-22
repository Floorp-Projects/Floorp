import hashlib
import json
from pathlib import Path
import socket
import tempfile
import unittest

from diagnostics import failure_diagnostic, MAX_DIAGNOSTIC
from protocol import BrowserPhaseError, Marionette, MAX_FRAME
import run


def fixture(profile='/owned-synthetic-profile'):
    marker=(json.dumps({'nonce':'a'*32,'case':'workspace-exit','head':'b'*40},indent=2)+'\n').encode()
    context={'floorpHead':'b'*40,'run':123,'attempt':1,'case':'workspace-exit','nonce':'a'*32,'boot':2,
             'profileIdentity':{'realpath':profile,'device':1,'inode':2,'markerSha256':hashlib.sha256(marker).hexdigest()},
             'expectedSyntheticProfile':profile,'nativePid':321,'runtimeVersion':'157.0.1','runtimeBuildID':'20261007125927',
             'expectedBootstrapRunId':'synthetic-boot2','expectedConfiguredNewTabURL':'http://localhost:5186/'}
    result={'ok':False,'syntheticProfileVerified':True,'phase':'verified','case':'workspace-exit','nonce':'a'*32,
            'error':'No unexpected restored tabs','observationStage':'verify-before-selection',
            'observedState':{'identity':{'profile':profile,'pid':321,'version':'157.0.1','buildID':'20261007125927'},
                             'bootstrapIsolation':{'runId':'synthetic-boot2','status':'done','aborted':False,'resultsCount':0,'discoveredCount':0,'startupTest':'loaded'},
                             'configuredNewTabURL':'http://localhost:5186/','homepage':'about:home','activeWorkspace':'workspace-a','pendingExit':False,
                             'tabs':[{'url':'https://synthetic-unexpected.invalid/','currentURI':'https://synthetic-unexpected.invalid/','workspace':'workspace-a','hidden':False,'selected':True}]}}
    return result,context,marker


class Diagnostics(unittest.TestCase):
    def test_exact_synthetic_uri_membership_selection_and_configuration_are_saved(self):
        result,context,marker=fixture()
        diagnostic,encoded=failure_diagnostic(result,context,marker)
        self.assertLessEqual(len(encoded),MAX_DIAGNOSTIC)
        self.assertEqual(json.loads(encoded),diagnostic)
        self.assertEqual(diagnostic['tabs'],result['observedState']['tabs'])
        self.assertEqual(diagnostic['configuredNewTabURL'],'http://localhost:5186/')
        self.assertEqual(diagnostic['expectedConfiguredNewTabURL'],'http://localhost:5186/')
        self.assertTrue(diagnostic['syntheticProfileOnly']);self.assertTrue(diagnostic['bootstrapRunMatches'])
        self.assertEqual(diagnostic['originalError'],result['error'])

    def test_unrelated_fields_are_not_serialized(self):
        result,context,marker=fixture()
        result['environment']={'TOKEN':'SYNTHETIC_DO_NOT_SAVE'}
        result['observedState']['identity']['secrets']='SYNTHETIC_DO_NOT_SAVE'
        result['observedState']['tabs'][0]['formData']='SYNTHETIC_DO_NOT_SAVE'
        result['observedState']['bootstrapIsolation']['cookies']='SYNTHETIC_DO_NOT_SAVE'
        _,encoded=failure_diagnostic(result,context,marker)
        self.assertNotIn(b'SYNTHETIC_DO_NOT_SAVE',encoded)

    def test_foreign_profile_pid_runtime_or_unverified_scope_reject(self):
        for change in ['profile','pid','runtime','scope','phase','nonce','case','marker','head','attempt','path']:
            result,context,marker=fixture()
            if change=='profile':result['observedState']['identity']['profile']='/foreign-profile'
            elif change=='pid':result['observedState']['identity']['pid']=999
            elif change=='runtime':result['observedState']['identity']['buildID']='foreign'
            elif change=='scope':result['syntheticProfileVerified']=False
            elif change=='phase':result['phase']='prepared'
            elif change=='nonce':result['nonce']='c'*32
            elif change=='case':result['case']='normal'
            elif change=='marker':marker=marker.replace(b'workspace-exit',b'normal')
            elif change=='head':context['floorpHead']='c'*40
            elif change=='attempt':context['attempt']=2
            else:context['expectedSyntheticProfile']='/foreign-profile'
            with self.subTest(change=change),self.assertRaises(ValueError):failure_diagnostic(result,context,marker)

    def test_invalid_tab_types_and_utf8_byte_bound_reject(self):
        result,context,marker=fixture()
        result['observedState']['tabs'][0]['selected']='true'
        with self.assertRaisesRegex(ValueError,'selection'):failure_diagnostic(result,context,marker)
        result,context,marker=fixture()
        result['observedState']['tabs'][0]['url']='日本語'*250000
        with self.assertRaisesRegex(ValueError,'byte bound'):failure_diagnostic(result,context,marker)

    def test_failed_bootstrap_is_diagnostic_only(self):
        result,context,marker=fixture()
        result['observedState']['bootstrapIsolation']['runId']='foreign-control'
        result['observedState']['bootstrapIsolation']['status']='error'
        diagnostic,_=failure_diagnostic(result,context,marker)
        self.assertFalse(diagnostic['bootstrapRunMatches'])
        self.assertEqual(diagnostic['bootstrapIsolation']['status'],'error')
        self.assertNotIn('PASS',json.dumps(diagnostic))


class DeferredTrace(unittest.TestCase):
    def client(self,result):
        a,b=socket.socketpair();a.setblocking(False)
        self.addCleanup(a.close);self.addCleanup(b.close)
        c=Marionette.__new__(Marionette);c.sock=a;c.index=0;c.pump=lambda:None;c.pending_phase_trace=None
        self.saved=[];c.trace=self.saved.append
        raw=json.dumps([1,0,None,{'value':result}]).encode()
        c.buffer=bytearray(str(len(raw)).encode()+b':'+raw)
        return c,raw

    def test_failure_frame_and_snapshot_are_deferred_and_not_in_exception(self):
        result,_,_=fixture();c,raw=self.client(result)
        with self.assertRaises(BrowserPhaseError) as caught:c.phase('synthetic script',{})
        self.assertEqual(caught.exception.result,result)
        self.assertIn(result['error'],str(caught.exception))
        self.assertNotIn('synthetic-unexpected',str(caught.exception))
        self.assertEqual(self.saved,[]);self.assertEqual(c.pending_phase_trace,raw)
        c.discard_phase_trace();self.assertEqual(self.saved,[])

    def test_accepted_phase_frame_is_preserved_exactly_once(self):
        result,_,_=fixture();result['ok']=True;c,raw=self.client(result)
        self.assertEqual(c.phase('synthetic script',{}),result);self.assertEqual(self.saved,[])
        c.accept_phase_trace();c.accept_phase_trace();self.assertEqual(self.saved,[raw])

    def test_framing_overhead_is_included_in_byte_bound(self):
        c,_=self.client({'ok':True})
        c.buffer=bytearray(str(MAX_FRAME).encode()+b':')
        with self.assertRaisesRegex(ValueError,'byte bound'):c.read(0,False)
        self.assertEqual(self.saved,[])


class PhasePersistence(unittest.TestCase):
    def exercise(self,invalid=False,write_failure=False):
        tmp=tempfile.TemporaryDirectory();self.addCleanup(tmp.cleanup)
        profile=Path(tmp.name)/'profile';profile.mkdir();out=Path(tmp.name)/'output';out.mkdir()
        result,context,marker=fixture(str(profile.resolve()))
        (profile/'qa3-restart-profile.json').write_bytes(marker)
        identity=run.profile_identity(profile)
        record={'case':'workspace-exit','nonce':'a'*32,'profileIdentity':identity}
        receipt={'floorpHead':'b'*40,'run':123,'attempt':1}
        args={'action':'verify','case':record['case'],'nonce':record['nonce'],'control':{'runId':'synthetic-boot2'},'expected':{'state':{'configuredNewTabURL':'http://localhost:5186/'}}}
        boot={'root':{'pid':321},'profileIdentity':identity};artifact={'version':'157.0.1','buildId':'20261007125927'}
        if invalid:result['observedState']['identity']['profile']='/foreign-profile'
        error=BrowserPhaseError(result)
        class Client:
            accepted=False;discarded=False;payload=None
            def phase(self,script,payload):self.payload=payload;raise error
            def accept_phase_trace(self):self.accepted=True
            def discard_phase_trace(self):self.discarded=True
        client=Client()
        target=out
        if write_failure:
            target=Path(tmp.name)/'not-a-directory';target.write_text('synthetic')
        with self.assertRaises(BrowserPhaseError) as caught:run.browser_phase(client,args,boot,artifact,profile,target,record,receipt)
        self.assertIs(caught.exception,error)
        self.assertTrue(client.discarded)
        self.assertEqual(client.payload['syntheticProfile'],identity)
        self.assertEqual(client.payload['nativePid'],321)
        return client,record,out,error

    def test_validated_failure_is_saved_without_replacing_original_error(self):
        client,record,out,error=self.exercise()
        self.assertTrue(client.accepted);self.assertNotIn('diagnosticError',record)
        saved=json.loads((out/record['failureDiagnosticEvidenceFile']).read_text())
        self.assertEqual(saved['originalError'],'No unexpected restored tabs')
        self.assertEqual(saved['tabs'][0]['currentURI'],'https://synthetic-unexpected.invalid/')
        self.assertNotIn('synthetic-unexpected',str(error))

    def test_foreign_snapshot_is_never_traced_or_saved(self):
        client,record,out,_=self.exercise(invalid=True)
        self.assertFalse(client.accepted);self.assertIn('diagnosticError',record)
        self.assertEqual(list(out.iterdir()),[])

    def test_diagnostic_io_failure_keeps_original_failure_and_discards_trace(self):
        client,record,_,error=self.exercise(write_failure=True)
        self.assertFalse(client.accepted);self.assertIn('diagnosticError',record)
        self.assertNotIn('failureDiagnosticEvidenceFile',record)
        self.assertEqual(str(error),'Original browser phase failed: No unexpected restored tabs')


if __name__=='__main__':unittest.main()
