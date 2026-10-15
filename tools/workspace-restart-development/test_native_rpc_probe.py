"""Host fixtures verify failure, preservation and closure; no native execution."""
import copy
import hashlib
import json
import subprocess
import unittest
import test_contract as prior
from test_rpc_diagnostics import OwnedFixtures, frame, SECRET
import run
import native_rpc_probe as probe_module


class NativeProbe(OwnedFixtures):
    def fixture(self, fault=None):
        scope,record,profile,out,owner=self.scope()
        parent=prior.Contract().receipt();parent.update(floorpHead=scope.receipt['floorpHead'],run=scope.receipt['run'])
        control={'schemaVersion':1,'purpose':'restart-bootstrap-isolation','runId':'qa3-restart-separate-probe-control','expiresAtMs':2000,'filter':[run.ISOLATION_TARGET]}
        boot={'root':owner['root'],'profileIdentity':record['profileIdentity'],'control':control,
              'binarySha256':parent['cases'][0]['boots'][0]['binarySha256'],'overlayBuildID2':'same-overlay'}
        record['boots']=[boot]
        proof={'ok':True,'profile':str(profile.resolve()),'pid':321,'version':'157.0.1','buildID':'20261007125927',
               'bootstrapIsolation':{'runId':control['runId'],'status':'done','results':[],'discoveredFiles':[],
                  'aborted':False,'completedAtMs':1000,'startupTest':'loaded'},'quitScheduled':True,'quitDelayMillis':15000}
        probe=scope.receipt;probe.update(schema='qa3-native-rpc-fault-probe-v1',scope='NATIVE_RPC_FAULT_INDUCTION_ONLY',status='RUNNING',
              record=record,sourceHashes=parent['sourceHashes'],runtimeVersion='157.0.1',runtimeBuildID='20261007125927',
              nativeProductQualification='UNVERIFIED',compileProfileQualification='UNVERIFIED',publicationAuthorized=False,
              injectionKind='NATIVE_ASYNC_EXCEPTION_BEFORE_CALLBACK',expectedRejection=False,productPassingCasesContributed=0,forcedCleanup=False)
        error={'error':'javascript error','message':'ReferenceError: restartOracle is not defined','stacktrace':SECRET}
        client,peer=self.client(scope=scope)
        client.buffer=bytearray(frame([1,0,None,{'value':proof}])+frame(fault or [1,1,error,None]))
        client.failure_scope=scope
        return client,scope,probe,parent,out

    def exercise(self,client,probe):
        probe_module.exercise_native_fault(client,probe,{'version':'157.0.1','buildId':'20261007125927'},run.validate_isolation)

    def complete(self,probe,parent,out):
        boot=probe['record']['boots'][0];boot.update(normalExit=True,exitCode=0,oldTreeGone=True,portGone=True,exitedBrowserIdentities={'321':'55'})
        probe.update(status=probe_module.VERIFIED,syntheticProfileRemoved=True)
        (out/'rpc-fault-probe-boot1-control.json').write_bytes(probe_module.encoded(run.control_evidence(boot['control'])))
        (out/'rpc-fault-probe-boot1-protocol.log').write_bytes(b'\n'.join(self.traces))
        probe['evidenceFiles']={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.iterdir()}
        parent['rpcFaultProbeRequested']=True;parent['rpcFaultProbe']=probe
        parent['evidenceFiles']['rpc-fault-probe-receipt.json']={'bytes':len(probe_module.encoded(probe)),'sha256':run.json_digest(probe)}

    def test_actual_protocol_error_closed_same_primary_separate_receipt(self):
        client,scope,probe,parent,out=self.fixture();self.exercise(client,probe)
        self.assertEqual(client.index,2);self.assertEqual(client.sock.fileno(),-1)
        self.assertNotIn('_rpcFailureRecord',probe);self.assertTrue(probe['originalExceptionSameIdentity'])
        self.assertNotIn(SECRET,json.dumps(probe)+b''.join(self.traces).decode())
        self.complete(probe,parent,out);run.validate_receipt(parent)
        self.assertEqual(len(parent['cases']),2);self.assertEqual(probe['productPassingCasesContributed'],0)
        self.assertFalse(probe['expectedRejection']);self.assertEqual(probe['diagnosticObservation']['status'],'FAIL_DIAGNOSTIC_ONLY')

    def test_successful_callback_cannot_substitute_for_induced_failure(self):
        client,_,probe,_,out=self.fixture([1,1,None,{'value':{'ok':True}}])
        with self.assertRaisesRegex(ValueError,'did not produce'):self.exercise(client,probe)
        self.assertNotEqual(probe['status'],probe_module.VERIFIED);self.assertFalse((out/probe_module.DIAGNOSTIC_NAME).exists())

    def test_wrong_code_detail_or_correlation_keeps_original_failure(self):
        for response in [[1,1,{'error':'script timeout','message':SECRET},None],
                         [1,1,{'error':'javascript error','message':SECRET},None],
                         [1,2,{'error':'javascript error','message':'restartOracle is not defined'},None]]:
            with self.subTest(response=response):
                client,scope,probe,_,_=self.fixture(response)
                with self.assertRaises(ValueError) as caught:self.exercise(client,probe)
                self.assertIs(caught.exception,scope.failure_observation['error']);self.assertEqual(client.index,2)
                self.assertNotIn(SECRET,json.dumps(probe));self.assertNotEqual(probe['status'],probe_module.VERIFIED)

    def test_owner_disappearance_before_diagnostic_is_not_native_fault_proof(self):
        client,scope,probe,_,_=self.fixture();original=scope.probe;calls=0
        def disappearing():
            nonlocal calls
            calls+=1
            return original() if calls<4 else {'status':'OWNED_NATIVE_EXITED','root':None,'portOwnerConfirmed':False}
        scope.probe=disappearing
        with self.assertRaises(ValueError):self.exercise(client,probe)
        self.assertTrue(probe['verificationError']);self.assertNotEqual(probe['status'],probe_module.VERIFIED)

    def test_symlink_or_public_diagnostic_not_read_as_private_proof(self):
        client,scope,probe,_,out=self.fixture();self.exercise(client,probe)
        path=out/probe_module.DIAGNOSTIC_NAME;path.chmod(0o644)
        with self.assertRaisesRegex(ValueError,'private'):probe_module.read_diagnostic(scope)
        path.unlink();other=out/'synthetic-secret.txt';other.write_text(SECRET);path.symlink_to(other)
        with self.assertRaises(OSError):probe_module.read_diagnostic(scope)

    def test_missing_skip_reuse_forced_changed_source_or_hash_fail_closed(self):
        client,_,probe,parent,out=self.fixture();self.exercise(client,probe);self.complete(probe,parent,out)
        for change in ['skip','count','expected','retry','source','force','exit','pid','nonce','bootstrap','hash','flag','original','extra']:
            r=copy.deepcopy(parent);p=r['rpcFaultProbe'];b=p['record']['boots'][0]
            if change=='skip':p['status']='UNEXECUTED'
            elif change=='count':p['productPassingCasesContributed']=1
            elif change=='expected':p['expectedRejection']=True
            elif change=='retry':p['postFailureRpcCount']=False
            elif change=='source':p['floorpHead']='f'*40
            elif change=='force':p['forcedCleanup']=True
            elif change=='exit':b['exitCode']=1
            elif change=='pid':b['root']=copy.deepcopy(r['cases'][0]['boots'][0]['root'])
            elif change=='nonce':p['record']['nonce']=r['cases'][0]['nonce']
            elif change=='bootstrap':p['record']['nativePreflight']['bootstrapIsolation']['results']=[{}]
            elif change=='hash':p['diagnosticFile']['sha256']='f'*64
            elif change=='flag':r['rpcFaultProbeRequested']=False
            elif change=='original':p['originalFault']=None
            else:p['diagnosticObservation']['arbitraryMessage']=SECRET
            with self.subTest(change=change),self.assertRaises(ValueError):run.validate_receipt(r)

    def test_actual_shared_native_scripts_under_owned_and_foreign_vm_fixtures(self):
        javascript=r"""
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const scripts=JSON.parse(fs.readFileSync(0,'utf8'));
const watchdog=setTimeout(()=>{throw new Error('Fixture completion deadline');},2000);
(async()=>{
  for(const kind of ['owned','foreign','contaminated']){
    let reads=0;const timers=[];
    const cfg={profile:'/owned/synthetic',pid:321,control:{schemaVersion:1,runId:'probe',expiresAtMs:2000,filter:['tools/workspace-restart-development/no-autodiscovered-suites.test.ts']}};
    const state={runId:'probe',status:'done',results:kind==='contaminated'?[{}]:[],discoveredFiles:[],aborted:false};
    const win={setTimeout(fn,delay){timers.push({fn,delay});}};
    const Services={dirsvc:{get(){return {path:kind==='foreign'?'/foreign':cfg.profile};}},appinfo:{processID:321,version:'157.0.1',appBuildID:'20261007125927'},
      wm:{getMostRecentWindow(){return win;}},prefs:{getStringPref(name){reads++;return name==='nora.tests.state'?JSON.stringify(state):'loaded';}},startup:{quit(){}}};
    let done;const result=new Promise(resolve=>done=resolve);
    vm.runInNewContext('(function(){'+scripts.preflight+'}).apply(null,[cfg,done]);',{cfg,done,Services,Components:{interfaces:{nsIFile:{},nsIAppStartup:{eAttemptQuit:1}}},Date:{now:()=>1000}},{timeout:1000});
    const value=await result;
    if(kind==='owned'){assert.equal(value.ok,true);assert.equal(timers.length,1);assert.equal(timers[0].delay,15000);assert.equal(value.bootstrapIsolation.results.length,0);assert.equal(value.bootstrapIsolation.completedAtMs,1000);}
    else{assert.equal(value.ok,false);assert.equal(timers.length,0);if(kind==='foreign')assert.equal(reads,0);}
  }
  let callback=false;
  assert.throws(()=>vm.runInNewContext(scripts.fault,{done:()=>callback=true},{timeout:1000}),{name:'ReferenceError',message:'restartOracle is not defined'});
  assert.equal(callback,false);clearTimeout(watchdog);process.stdout.write('SHARED_NATIVE_SCRIPT_FIXTURES_VERIFIED\n');
})().catch(error=>{clearTimeout(watchdog);console.error(error);process.exitCode=1;});
"""
        result=subprocess.run(['node','-e',javascript],input=json.dumps({'preflight':probe_module.PREFLIGHT_SCRIPT,'fault':probe_module.FAULT_SCRIPT}),text=True,capture_output=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr);self.assertIn('SHARED_NATIVE_SCRIPT_FIXTURES_VERIFIED',result.stdout)


if __name__=='__main__':unittest.main()
