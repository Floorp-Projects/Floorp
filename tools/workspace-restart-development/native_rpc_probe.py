"""Opt-in real native fault induction, separate from cases and expected rejection."""
import hashlib
import json
import os
import stat
from rpc_diagnostics import MAX_RPC_DIAGNOSTIC, exception_type, failure_text, verify_profile

PREFLIGHT_SCRIPT = r"""
const cfg=arguments[0], done=arguments[arguments.length-1];
(async()=>{
  const profile=Services.dirsvc.get('ProfD',Components.interfaces.nsIFile).path;
  const pid=Services.appinfo.processID;
  if(profile!==cfg.profile || pid!==cfg.pid) throw new Error('Owned probe identity differs');
  const win=Services.wm.getMostRecentWindow('navigator:browser');
  if(!win) throw new Error('Owned probe window unavailable');
  const control=cfg.control;
  if(control.schemaVersion!==1 || typeof control.runId!=='string' ||
     control.filter.length!==1 || control.filter[0]!=='tools/workspace-restart-development/no-autodiscovered-suites.test.ts')
    throw new Error('Owned probe control differs');
  const deadline=Date.now()+30000;
  while(Date.now()<deadline && Date.now()<control.expiresAtMs){
    const text=Services.prefs.getStringPref('nora.tests.state','');
    if(text.length>8192) throw new Error('Owned probe bootstrap bound');
    if(text){
      const state=JSON.parse(text);
      if(state.runId!==control.runId || state.error || state.abortReason ||
         !Array.isArray(state.results) || state.results.length!==0 ||
         !Array.isArray(state.discoveredFiles) || state.discoveredFiles.length!==0 || state.status==='error')
        throw new Error('Owned probe bootstrap differs');
      const startupTest=Services.prefs.getStringPref('nora.startup.test','');
      if(state.status==='done' && state.aborted===false && startupTest==='loaded'){
        win.setTimeout(()=>Services.startup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit),15000);
        return {ok:true,profile,pid,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,
          bootstrapIsolation:{runId:state.runId,status:state.status,results:state.results,
            discoveredFiles:state.discoveredFiles,aborted:state.aborted,completedAtMs:Date.now(),startupTest},
          quitScheduled:true,quitDelayMillis:15000};
      }
    }
    await new Promise(resolve=>win.setTimeout(resolve,100));
  }
  throw new Error('Owned probe bootstrap deadline');
})().then(done,()=>done({ok:false}));
"""
FAULT_SCRIPT = "throw new ReferenceError('restartOracle is not defined');"
DIAGNOSTIC_NAME = 'normal-boot1-rpc-failure-diagnostic.json'
VERIFIED = 'FAULT_INDUCED_DIAGNOSTIC_VERIFIED'

def encoded(value):return (json.dumps(value,indent=2)+'\n').encode()

def verify_preflight(value,record,boot,artifact,validate_isolation):
    keys={'ok','profile','pid','version','buildID','bootstrapIsolation','quitScheduled','quitDelayMillis'}
    if type(value) is not dict or set(value)!=keys or value['ok'] is not True:raise ValueError('Owned native probe preflight unavailable')
    if value['profile']!=record['profileIdentity']['realpath'] or type(value['pid']) is not int or value['pid']!=boot['root']['pid']:raise ValueError('Owned native probe ProfD/PID differs')
    if value['version']!=artifact['version'] or value['buildID']!=artifact['buildId']:raise ValueError('Owned native probe Runtime differs')
    validate_isolation(value['bootstrapIsolation'],boot['control'])
    if value['quitScheduled'] is not True or type(value['quitDelayMillis']) is not int or value['quitDelayMillis']!=15000:raise ValueError('Owned native probe graceful quit was not acknowledged')
    return value

def validate_diagnostic(value,probe):
    record=probe['record'];boot=record['boots'][0]
    expected={'schema':'qa3-workspace-restart-owned-rpc-failure-v1','scope':'ORDINARY_DEVELOPMENT_E2E',
      'status':'FAIL_DIAGNOSTIC_ONLY','syntheticProfileOnly':True,'floorpHead':probe['floorpHead'],'run':probe['run'],'attempt':1,
      'case':'normal','nonce':record['nonce'],'boot':1,'operation':'phase','originalExceptionType':'ValueError','originalErrno':None,
      'profileIdentity':record['profileIdentity'],'runtimeExpected':{'version':probe['runtimeVersion'],'buildID':probe['runtimeBuildID']},
      'ownershipProofKind':'HOST_MARKER_AND_PROCESS_ONLY','nativeProfDVerifiedByDiagnostic':False,'rpcMetadataAvailability':'AVAILABLE',
      'nativeSnapshotCollected':False,'nativeSnapshotAvailability':'UNAVAILABLE_HOST_DIAGNOSTIC_ONLY','arbitraryErrorTextWithheld':True,
      'retryPerformed':False,'nativeProductQualification':'UNVERIFIED','compileProfileQualification':'UNVERIFIED','publicationAuthorized':False}
    if type(value) is not dict or set(value)!=set(expected)|{'processOwnership','rpc'}:raise ValueError('Closed native RPC diagnostic schema required')
    if any(type(value[k]) is not type(v) or value[k]!=v for k,v in expected.items()):raise ValueError('Original owned native fault context differs')
    owner={'status':'VERIFIED_LIVE_NATIVE','root':{k:boot['root'][k] for k in ['pid','start','session']},'portOwnerConfirmed':True}
    if value['processOwnership']!=owner:raise ValueError('Native owner disappeared before diagnostic persistence')
    rpc=value['rpc'];keys={'stage','command','requestId','bytesSent','receivedBytes','frameBytes','responseCorrelated','nativeError','secondaryCloseExceptionType'}
    if type(rpc) is not dict or set(rpc)!=keys or rpc['stage']!='native-error' or rpc['command']!='WebDriver:ExecuteAsyncScript':raise ValueError('Actual correlated native async exception required')
    if type(rpc['requestId']) is not int or rpc['requestId']!=probe['expectedFaultRequestId'] or rpc['responseCorrelated'] is not True:raise ValueError('Original fault request correlation required')
    if rpc['nativeError']!={'code':'javascript error','detail':'ReferenceError: restartOracle is not defined','detailWithheld':False} or rpc['secondaryCloseExceptionType'] is not None:raise ValueError('Exact injected native exception without secondary close failure required')
    for field in ['bytesSent','receivedBytes','frameBytes']:
        if type(rpc[field]) is not int or not 0<=rpc[field]<=2*1024*1024:raise ValueError('Bounded original fault frame counters required')
    if rpc['bytesSent']==0 or rpc['frameBytes']==0:raise ValueError('Actual native fault frame evidence missing')

def read_diagnostic(scope):
    verify_profile(scope.profile,scope.identity,scope.context)
    directory=os.open(scope.output,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        fd=os.open(DIAGNOSTIC_NAME,os.O_RDONLY|os.O_NOFOLLOW,dir_fd=directory)
        with os.fdopen(fd,'rb') as stream:
            row=os.fstat(stream.fileno())
            if not stat.S_ISREG(row.st_mode) or stat.S_IMODE(row.st_mode)!=0o600 or not 0<row.st_size<=MAX_RPC_DIAGNOSTIC:raise ValueError('Bounded private native fault diagnostic required')
            raw=stream.read(MAX_RPC_DIAGNOSTIC+1)
            if len(raw)!=row.st_size:raise ValueError('Native fault diagnostic changed during read')
    finally:os.close(directory)
    verify_profile(scope.profile,scope.identity,scope.context)
    value=json.loads(raw)
    if raw!=encoded(value):raise ValueError('Original native diagnostic encoding differs')
    return value,{'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}

def exercise_native_fault(client,probe,artifact,validate_isolation):
    record=probe['record'];boot=record['boots'][0];scope=client.failure_scope
    with scope.guard('phase'):
        value=client.command('WebDriver:ExecuteAsyncScript',{'script':PREFLIGHT_SCRIPT,'args':[{'profile':record['profileIdentity']['realpath'],'pid':boot['root']['pid'],'control':boot['control']}]},35)
        scope.check('phase')
        preflight=verify_preflight(value.get('value') if type(value) is dict else None,record,boot,artifact,validate_isolation)
    record['nativePreflight']=preflight
    if (scope.profile/'nora-tests-control.json').exists():raise ValueError('Native probe control was not consumed')
    probe['expectedFaultRequestId']=client.index
    try:
        with scope.guard('phase'):
            client.command('WebDriver:ExecuteAsyncScript',{'script':FAULT_SCRIPT,'args':[]},5,trace_response=False)
    except BaseException as error:
        observation=probe.pop('_rpcFailureRecord',None)
        probe['originalFault']=failure_text(error,observation)
        probe['originalExceptionSameIdentity']=type(observation) is dict and observation.get('error') is error and scope.failure_observation is observation
        try:
            if type(error) is not ValueError or probe['originalExceptionSameIdentity'] is not True or record.get('rpcDiagnosticError'):raise ValueError('Original native exception or safe diagnostic unavailable')
            value,inventory=read_diagnostic(scope);validate_diagnostic(value,probe)
            if record.get('rpcFailureDiagnosticEvidenceFile')!=DIAGNOSTIC_NAME:raise ValueError('Original diagnostic file identity missing')
            probe['diagnosticObservation']=value;probe['diagnosticFile']=dict(inventory,name=DIAGNOSTIC_NAME)
            probe['inducedRpcFailureObserved']=True;probe['faultOperationOutcome']='FAIL_DIAGNOSTIC_ONLY'
            probe['requestCounterAfterFault']=client.index
            probe['postFailureRpcCount']=client.index-probe['expectedFaultRequestId']-1;probe['status']='FAULT_OBSERVED_DIAGNOSTIC_RECORDED'
        except BaseException as verification:
            probe['verificationError']='Native fault verification failed: '+exception_type(verification)
            raise error
    else:raise ValueError('Native probe did not produce the injected exception')

def validate_probe(probe,parent,validate_isolation):
    if probe.get('schema')!='qa3-native-rpc-fault-probe-v1' or probe.get('scope')!='NATIVE_RPC_FAULT_INDUCTION_ONLY' or probe.get('status')!=VERIFIED:raise ValueError('Completed separate native fault proof required')
    for key in ['floorpHead','run','attempt','sourceHashes','runtimeVersion','runtimeBuildID']:
        if probe.get(key)!=parent[key]:raise ValueError('Native fault source/run/Runtime differs')
    if probe.get('expectedRejection') is not False or type(probe.get('productPassingCasesContributed')) is not int or probe['productPassingCasesContributed']!=0 or probe.get('injectionKind')!='NATIVE_ASYNC_EXCEPTION_BEFORE_CALLBACK':raise ValueError('Fault induction is separate from expected rejection and product passes')
    if any(probe.get(k) is not True for k in ['inducedRpcFailureObserved','originalExceptionSameIdentity','syntheticProfileRemoved']) or probe.get('forcedCleanup') is not False or probe.get('verificationError') or type(probe.get('postFailureRpcCount')) is not int or probe['postFailureRpcCount']!=0:raise ValueError('Missing original fault or normal cleanup proof')
    if probe.get('faultOperationOutcome')!='FAIL_DIAGNOSTIC_ONLY' or probe.get('originalFault')!='Original RPC failure: ValueError [native-error] WebDriver:ExecuteAsyncScript':raise ValueError('Original induced RPC failure must remain a failure')
    if type(probe.get('requestCounterAfterFault')) is not int or probe['requestCounterAfterFault']!=probe['expectedFaultRequestId']+1:raise ValueError('One original fault operation without a retry required')
    if probe.get('nativeProductQualification')!='UNVERIFIED' or probe.get('compileProfileQualification')!='UNVERIFIED' or probe.get('publicationAuthorized') is not False:raise ValueError('Native fault cannot qualify product publication')
    record=probe['record'];boots=record.get('boots',[])
    if record.get('case')!='normal' or len(boots)!=1:raise ValueError('One separately marked native probe boot required')
    boot=boots[0];root=boot['root']
    if type(root['pid']) is not int or type(root['session']) is not int or root['pid']!=root['session'] or not root['start'].isdigit():raise ValueError('Independent native fault generation required')
    if any(boot.get(k) is not True for k in ['normalExit','oldTreeGone','portGone']) or type(boot.get('exitCode')) is not int or boot['exitCode']!=0 or boot.get('exitedBrowserIdentities',{}).get(str(root['pid']))!=root['start']:raise ValueError('Native fault process did not quit normally')
    others=[b for case in parent['cases'] for b in case['boots']]
    if (root['pid'],root['start']) in [(b['root']['pid'],b['root']['start']) for b in others] or boot['control']['runId'] in [b['control']['runId'] for b in others] or record['nonce'] in [c['nonce'] for c in parent['cases']]:raise ValueError('Fault induction reused an original case identity')
    if boot['binarySha256']!=others[0]['binarySha256'] or boot['overlayBuildID2']!=others[0]['overlayBuildID2'] or boot['profileIdentity']!=record['profileIdentity']:raise ValueError('Fault induction changed Runtime/overlay/profile')
    verify_preflight(record['nativePreflight'],record,boot,{'version':parent['runtimeVersion'],'buildId':parent['runtimeBuildID']},validate_isolation)
    validate_diagnostic(probe['diagnosticObservation'],probe)
    if probe.get('diagnosticFile')!={'name':DIAGNOSTIC_NAME,**probe['evidenceFiles'].get(DIAGNOSTIC_NAME,{})} or probe['diagnosticFile']['sha256']!=hashlib.sha256(encoded(probe['diagnosticObservation'])).hexdigest():raise ValueError('Native diagnostic byte/hash inventory differs')
    for name in [DIAGNOSTIC_NAME,'rpc-fault-probe-boot1-protocol.log','rpc-fault-probe-boot1-control.json']:
        if probe['evidenceFiles'].get(name,{}).get('bytes',0)<=0:raise ValueError('Original native probe evidence missing')
