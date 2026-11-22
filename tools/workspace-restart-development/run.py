"""Two development-only cases, each with two real boots of one synthetic profile."""
import argparse
import configparser
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import time
from urllib.parse import unquote
import uuid

from diagnostics import failure_diagnostic
from protocol import BrowserPhaseError, Marionette
from rpc_diagnostics import RpcFailureScope, exception_type, failure_text
from native_rpc_probe import exercise_native_fault, validate_probe, VERIFIED
from session import decode, validate_saved

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).parent
CASES=json.loads((HERE/'cases.json').read_text())
LIMIT=8*1024*1024
PORT=2828
ISOLATION_TARGET='tools/workspace-restart-development/no-autodiscovered-suites.test.ts'
SOURCE_PATHS=[
    'tools/workspace-restart-development/'+name for name in ['run.py','protocol.py','session.py','diagnostics.py','rpc_diagnostics.py','native_rpc_probe.py','restore-oracle.js.txt','browser-phase.js.txt','cases.json']
]+['tools/src/browser_launcher.ts','tools/feles-build.ts','floorp-runtime.lock.json','tools/workspace-development/run.py','bridge/startup/src/chrome_root.ts','bridge/loader-features/loader/test/index.ts']
legacy_spec=importlib.util.spec_from_file_location('owned_six',ROOT/'tools/workspace-development/run.py')
legacy=importlib.util.module_from_spec(legacy_spec);legacy_spec.loader.exec_module(legacy)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write_json(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def json_digest(value):return hashlib.sha256((json.dumps(value,indent=2)+'\n').encode()).hexdigest()
def is_hash(value):return isinstance(value,str) and re.fullmatch('[0-9a-f]{64}',value) is not None

def browser_script():
    return 'const restartOracle = '+(HERE/'restore-oracle.js.txt').read_bytes().decode('utf-8')+';\n'+(HERE/'browser-phase.js.txt').read_bytes().decode('utf-8')

def browser_phase(client,args,boot,artifact,profile,out,record,receipt):
    if profile_identity(profile)!=record['profileIdentity']:
        raise ValueError('Original synthetic profile identity changed before phase')
    marker=(profile/'qa3-restart-profile.json').read_bytes()
    if json.loads(marker)!={'nonce':record['nonce'],'case':record['case'],'head':receipt['floorpHead']}:
        raise ValueError('Original synthetic profile marker changed before phase')
    payload={**args,'syntheticProfile':record['profileIdentity'],'nativePid':boot['root']['pid']}
    try:
        scope=getattr(client,'failure_scope',None)
        if scope is not None:scope.check('phase')
        result=client.phase(browser_script(),payload)
        if profile_identity(profile)!=record['profileIdentity']:
            raise ValueError('Synthetic profile identity changed during successful phase')
        if result.get('syntheticProfileVerified') is not True:
            raise ValueError('Successful phase lacks verified synthetic profile')
        validate_phase(result,args['action'],record['case'],record['nonce'],args['control'],boot,artifact)
        client.accept_phase_trace()
        return result
    except BrowserPhaseError as error:
        try:
            if profile_identity(profile)!=record['profileIdentity']:
                raise ValueError('Synthetic profile identity changed during failed phase')
            context={
                **{key:receipt[key] for key in ['floorpHead','run','attempt']},
                'case':record['case'],'nonce':record['nonce'],'boot':1 if args['action']=='prepare' else 2,
                'profileIdentity':record['profileIdentity'],'expectedSyntheticProfile':str(profile.resolve()),
                'nativePid':boot['root']['pid'],'runtimeVersion':artifact['version'],'runtimeBuildID':artifact['buildId'],
                'expectedBootstrapRunId':args['control']['runId'],
                'expectedConfiguredNewTabURL':args.get('expected',{}).get('state',{}).get('configuredNewTabURL'),
            }
            _,encoded=failure_diagnostic(error.result,context,(profile/'qa3-restart-profile.json').read_bytes())
            name=record['case']+'-boot'+str(context['boot'])+'-failure-diagnostic.json'
            with (out/name).open('xb') as f:f.write(encoded)
            record['failureDiagnosticEvidenceFile']=name
            client.accept_phase_trace()
        except BaseException as diagnostic_error:
            record['diagnosticError']=str(diagnostic_error)
        raise
    except BaseException as error:
        scope=getattr(client,'failure_scope',None)
        if scope is not None:scope.record_failure('phase',error)
        raise
    finally:
        client.discard_phase_trace()

def profile_identity(profile):
    if profile.is_symlink() or not profile.is_dir():raise ValueError('Synthetic profile is not the owned directory')
    s=profile.stat();marker=profile/'qa3-restart-profile.json'
    if marker.is_symlink() or not marker.is_file() or marker.stat().st_size>4096:raise ValueError('Original bounded profile marker missing')
    return {'realpath':str(profile.resolve()),'device':s.st_dev,'inode':s.st_ino,'markerSha256':digest(marker)}

def validate_boot_control(control,now_ms):
    if control.get('schemaVersion')!=1 or not isinstance(control.get('runId'),str) or not control['runId'] or control.get('filter')!=[ISOLATION_TARGET]:raise ValueError('Exact nonempty bootstrap isolation selector required')
    expires=control.get('expiresAtMs')
    if type(expires) is not int or expires<=now_ms or expires>now_ms+300000:raise ValueError('Fresh bounded bootstrap control lease required')

def control_pref_values(control):
    return {'nora.tests.filter':json.dumps(control['filter']),'nora.tests.filter.count':'1','nora.tests.filter.0':ISOLATION_TARGET,'nora.tests.run_id':control['runId'],'nora.tests.state':''}

def control_evidence(control):
    return {'control':control,'matchingPrefs':control_pref_values(control),'purpose':'BOOTSTRAP_ISOLATION_ZERO_COLOCATED_SUITES_NOT_A_QA_CASE'}

def write_boot_control(profile,caseid,number,run):
    if (ROOT/ISOLATION_TARGET).exists():raise ValueError('Bootstrap isolation target must remain undiscovered')
    now=int(time.time()*1000)
    control={'schemaVersion':1,'runId':f'qa3-restart-{run}-{caseid}-boot{number}-{uuid.uuid4().hex}','expiresAtMs':now+300000,'filter':[ISOLATION_TARGET]}
    validate_boot_control(control,now)
    values=control_pref_values(control)
    prefs=profile/'prefs.js';original=prefs.read_text()
    original=re.sub(r'^user_pref\("nora\.tests\.(?:filter(?:\.(?:count|\d+))?|run_id|state)",.*\);\s*$', '',original,flags=re.M)
    prefs.write_text(original.rstrip()+'\n'+''.join('user_pref('+json.dumps(k)+', '+json.dumps(v)+');\n' for k,v in values.items()))
    path=profile/'nora-tests-control.json';write_json(path,control);os.chmod(path,0o600)
    if json.loads(path.read_text())!=control:raise ValueError('Original bootstrap control read-back differs')
    readback={}
    for line in prefs.read_text().splitlines():
        m=re.fullmatch(r'user_pref\(("nora\.tests\.[^"]+"), (.*)\);',line)
        if m:readback[json.loads(m[1])]=json.loads(m[2])
    if readback!=values:raise ValueError('Original bootstrap control prefs read-back differs')
    return control,{k:readback[k] for k in values}

def validate_isolation(state,control):
    if control.get('schemaVersion')!=1 or control.get('filter')!=[ISOLATION_TARGET] or not isinstance(control.get('runId'),str) or not control['runId']:raise ValueError('Missing exact bootstrap isolation control')
    if state.get('runId')!=control['runId'] or state.get('status')!='done' or state.get('results')!=[] or state.get('discoveredFiles')!=[] or state.get('aborted') is not False or state.get('error') or state.get('abortReason') or state.get('startupTest')!='loaded':raise ValueError('Automatic suite bootstrap is foreign/incomplete/non-isolated')
    complete=state.get('completedAtMs');expires=control.get('expiresAtMs')
    if type(complete) is not int or type(expires) is not int or complete<=0 or complete>=expires:raise ValueError('Original native bootstrap lease did not complete in time')

def validate_phase(result,action,caseid,nonce,control,boot,artifact):
    if result.get('phase')!=('prepared' if action=='prepare' else 'verified') or result.get('case')!=caseid or result.get('nonce')!=nonce:raise ValueError('Original native case/nonce/phase identity differs')
    identity=result['state']['identity'];validate_isolation(identity['bootstrapIsolation'],control)
    if identity['pid']!=boot['root']['pid'] or identity['profile']!=boot['profileIdentity']['realpath'] or identity['buildID']!=artifact['buildId'] or identity['version']!=artifact['version'] or identity.get('restoreCompleted') is not True:raise ValueError('Original native source/profile/Runtime identity differs')
    return identity

def live(table,identities):
    return {pid for pid,start in identities.items() if pid in table and table[pid]['start']==start and table[pid]['state']!='Z'}

def present(table,identities):
    return {pid for pid,start in identities.items() if pid in table and table[pid]['start']==start}

def signal_owned(pid,start,sig):
    fd=os.pidfd_open(pid)
    try:
        table=legacy.process_table()
        if pid in table and table[pid]['start']==start:signal.pidfd_send_signal(fd,sig)
    finally:os.close(fd)

def capture_browser(table,tracked,root):
    # An independently owned native session also retains reparented children.
    # A shared server session is never adopted as native browser ownership.
    active=live(table,tracked)
    own_session=root['pid'] if root['session']==root['pid'] else None
    if own_session is not None and root['pid'] in table and table[root['pid']]['start']!=root['start']:raise ValueError('Native session leader generation reused before shutdown boundary')
    changed=True
    while changed:
        changed=False
        for pid,row in table.items():
            if pid in tracked and tracked[pid]!=row['start']:continue
            in_session=own_session is not None and row['session']==own_session
            if pid not in tracked and (row['ppid'] in active or in_session):
                tracked[pid]=row['start'];changed=True
                if row['state']!='Z':active.add(pid)
    return active

def merge_native_ownership(owned,tracked):
    for pid,start in tracked.items():
        if pid in owned and owned[pid]!=start:raise ValueError('Native/global owned generation collision')
        owned[pid]=start

def listeners():
    rows=[]
    for name,allowed in [('tcp',{'0100007F'}),('tcp6',{'00000000000000000000000001000000'})]:
        for line in (Path('/proc/net')/name).read_text().splitlines()[1:]:
            f=line.split();address,port=f[1].split(':')
            if int(port,16)==PORT and f[3]=='0A':
                if address not in allowed:raise ValueError('Marionette listener is not loopback')
                rows.append({'inode':f[9],'address':address,'port':PORT})
    return rows

def listener_owner(rows,table,owned,binary,profile):
    sockets={'socket:['+x['inode']+']' for x in rows};matches=[]
    for pid in live(table,owned):
        try:
            fd=Path('/proc')/str(pid)/'fd'
            if not any(os.readlink(f) in sockets for f in fd.iterdir()):continue
            exe=Path(os.readlink(Path('/proc')/str(pid)/'exe')).resolve()
            args=(Path('/proc')/str(pid)/'cmdline').read_bytes().split(b'\0')
            if exe!=binary.resolve() or b'--profile' not in args:raise ValueError('Original listener executable identity mismatch')
            at=args.index(b'--profile')
            if Path(os.fsdecode(args[at+1])).resolve()!=profile.resolve():raise ValueError('Original listener profile mismatch')
            matches.append({'pid':pid,'start':table[pid]['start'],'session':table[pid]['session'],'executable':str(exe),'profile':str(profile.resolve()),'listeners':rows})
        except (FileNotFoundError,ProcessLookupError):continue
    if len(matches)!=1:raise ValueError('Original listener does not have one owned native root')
    return matches[0]

def rpc_process_ownership(capture,owned,binary,profile,expected_root=None,inspect=None,port_owner=None):
    """Read only controller-owned generations; emit no argv/exe/foreign details."""
    def inspect_native(pid):
        directory=os.open(Path('/proc')/str(pid),os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        try:
            fd=os.open('stat',os.O_RDONLY,dir_fd=directory)
            try:identity=os.read(fd,4097)
            finally:os.close(fd)
            if len(identity)>4096 or identity[identity.rfind(b')')+2:].split()[19].decode()!=owned[pid]:
                raise ValueError('Owned process generation changed before inspection')
            exe=Path(os.readlink('exe',dir_fd=directory)).resolve()
            if exe!=binary.resolve():return exe,[]
            fd=os.open('cmdline',os.O_RDONLY,dir_fd=directory)
            try:args=os.read(fd,65537)
            finally:os.close(fd)
            if len(args)>65536:raise ValueError('Owned process argument byte bound')
            return exe,args.split(b'\0')
        finally:os.close(directory)
    inspect=inspect or inspect_native
    try:
        table=capture();matches=[]
        if expected_root is not None:
            pid=expected_root['pid']
            if pid not in table:return {'status':'OWNED_NATIVE_EXITED'}
            if table[pid]['start']!=expected_root['start'] or owned.get(pid)!=expected_root['start']:
                return {'status':'OWNERSHIP_REJECTED'}
            candidates=[pid]
        else:candidates=live(table,owned)
        for pid in candidates:
            row=table[pid]
            if row['state']=='Z':continue
            exe,args=inspect(pid)
            if exe!=binary.resolve():continue
            if b'--profile' not in args:continue
            at=args.index(b'--profile')
            if at+1>=len(args) or os.fsdecode(args[at+1])!=str(profile.resolve()) or row['session']!=pid:continue
            matches.append({'pid':pid,'start':row['start'],'session':row['session'],'profile':str(profile.resolve())})
        if len(matches)!=1:return {'status':'OWNERSHIP_REJECTED' if expected_root else 'NO_OWNED_NATIVE'}
        root=matches[0];after=capture()
        if root['pid'] not in after or after[root['pid']]['start']!=root['start'] or owned.get(root['pid'])!=root['start'] or after[root['pid']]['session']!=root['session']:
            return {'status':'OWNERSHIP_REJECTED'}
        confirmed=False
        if port_owner is not None:
            listener=port_owner()
            confirmed=listener is not None and all(listener.get(key)==root[key] for key in ['pid','start','session','profile'])
        return {'status':'VERIFIED_LIVE_NATIVE','root':root,'portOwnerConfirmed':confirmed}
    except Exception:return {'status':'PROBE_UNAVAILABLE'}

def valid_case(case):
    if case.get('status')!='PASS_DEVELOPMENT_ONLY' or len(case.get('boots',[]))!=2:raise ValueError('Missing or failed real restart case')
    if case.get('rpcFailureDiagnosticEvidenceFile') or case.get('rpcDiagnosticError'):raise ValueError('RPC failure diagnostics cannot qualify a passing case')
    first,second=case['boots']
    if not isinstance(case.get('nonce'),str) or re.fullmatch('[0-9a-f]{32}',case['nonce']) is None:raise ValueError('Fresh synthetic case nonce required')
    if (first['root']['pid'],first['root']['start'])==(second['root']['pid'],second['root']['start']):raise ValueError('Same process was reused instead of restarted')
    if first['profileIdentity']!=second['profileIdentity']:raise ValueError('Second boot changed synthetic profile')
    if any(case.get(key) is not True for key in ['oldBrowserTreeGoneBeforeSecondBoot','oldPortGoneBeforeSecondBoot','cleanSessionSaved']):raise ValueError('Missing shutdown/session boundary')
    for boot in case['boots']:
        if any(boot.get(key) is not True for key in ['normalExit','oldTreeGone','portGone']) or type(boot.get('exitCode')) is not int or boot['exitCode']!=0:raise ValueError('Forced/incomplete quit cannot qualify')
        if type(boot['root']['pid']) is not int or boot['root']['pid']<=0 or not isinstance(boot['root']['start'],str) or not boot['root']['start'].isdigit():raise ValueError('Original native generation identity missing')
        if boot['native']['profile']!=boot['profileIdentity']['realpath'] or boot['native']['pid']!=boot['root']['pid'] or boot['native'].get('restoreCompleted') is not True:raise ValueError('Native boot identity/restore mismatch')
        validate_isolation(boot['native']['bootstrapIsolation'],boot['control'])
        if boot['root']['session']!=boot['root']['pid'] or boot.get('exitedBrowserIdentities',{}).get(str(boot['root']['pid']))!=boot['root']['start']:raise ValueError('Independent native root disappearance inventory missing')
        saved=boot.get('cleanSessionFile',{})
        if type(saved.get('bytes')) is not int or not 12<=saved['bytes']<=4*1024*1024 or not is_hash(saved.get('sha256')):raise ValueError('Bounded original clean session evidence missing')
    if first['control']['runId']==second['control']['runId']:raise ValueError('Bootstrap control reused across native boots')
    if first['binarySha256']!=second['binarySha256'] or first['overlayBuildID2']!=second['overlayBuildID2'] or first['native']['buildID']!=second['native']['buildID']:raise ValueError('Runtime/build changed across restart')
    if not is_hash(first['binarySha256']) or not first['overlayBuildID2']:raise ValueError('Native/overlay executable identity missing')
    if case.get('profileIdentity')!=first['profileIdentity'] or not is_hash(first['profileIdentity'].get('markerSha256')):raise ValueError('Original marked case profile differs')
    if case.get('cleanSessionBeforeSecondBootSha256')!=first['cleanSessionFile']['sha256'] or case.get('consumedOriginalCleanSessionSha256')!=first['cleanSessionFile']['sha256']:raise ValueError('Original saved session consumption chain differs')
    for field,phase,boot in [('prepared','prepared',first),('verified','verified',second)]:
        value=case.get(field,{})
        if value.get('ok') is not True or value.get('case')!=case['case'] or value.get('nonce')!=case['nonce'] or value.get('phase')!=phase or value.get('state',{}).get('identity')!=boot['native']:raise ValueError('Original case/nonce/phase/native reply binding differs')

def validate_receipt(receipt):
    if receipt.get('schema')!='qa3-workspace-restart-development-v1' or receipt.get('scope')!='ORDINARY_DEVELOPMENT_E2E' or receipt.get('status')!='PASS_DEVELOPMENT_ONLY':raise ValueError('Exact development receipt scope/status required')
    if type(receipt.get('attempt')) is not int or receipt['attempt']!=1 or type(receipt.get('run')) is not int or receipt['run']<=0 or not isinstance(receipt.get('floorpHead'),str) or re.fullmatch('[0-9a-f]{40}',receipt['floorpHead']) is None:raise ValueError('Original source/run/attempt context required')
    if not isinstance(receipt.get('runtimeSource'),str) or re.fullmatch('[0-9a-f]{40}',receipt['runtimeSource']) is None or not receipt.get('runtimeArtifact') or not receipt.get('runtimeVersion') or re.fullmatch('[0-9]{14}',receipt.get('runtimeBuildID','')) is None:raise ValueError('Original locked Runtime context required')
    hashes=receipt.get('sourceHashes',{})
    if set(hashes)!=set(SOURCE_PATHS) or not all(is_hash(value) for value in hashes.values()):raise ValueError('Complete original source hash inventory required')
    if receipt.get('nativeProductQualification')!='UNVERIFIED' or receipt.get('compileProfileQualification')!='UNVERIFIED' or receipt.get('publicationAuthorized') is not False or receipt.get('nativeForcedCleanup') is not False:raise ValueError('Development receipt cannot authorize qualification/publication/forced native cleanup')
    rows=receipt.get('cases',[])
    if receipt.get('expectedCaseCount')!=2 or [row.get('case') for row in rows]!=list(CASES):raise ValueError('Exact two-case ordered identity required')
    for i,(row,name) in enumerate(zip(rows,CASES.values())):
        if row.get('name')!=name or type(row.get('index')) is not int or row['index']!=i+1 or row.get('syntheticProfileRemoved') is not True or row.get('firstError'):raise ValueError('Missing/foreign/failed restart case or cleanup')
        valid_case(row)
    boots=[boot for row in rows for boot in row['boots']]
    if len({(b['root']['pid'],b['root']['start']) for b in boots})!=4 or len({b['control']['runId'] for b in boots})!=4:raise ValueError('Four distinct real native generations/controls required')
    if len({r['nonce'] for r in rows})!=2 or len({r['profileIdentity']['markerSha256'] for r in rows})!=2:raise ValueError('Two separately fresh synthetic case profiles required')
    if len({b['binarySha256'] for b in boots})!=1 or len({b['overlayBuildID2'] for b in boots})!=1:raise ValueError('Native/overlay changed across the four boots')
    files=receipt.get('evidenceFiles',{})
    for name,row in files.items():
        if Path(name).name!=name or not isinstance(row,dict) or type(row.get('bytes')) is not int or row['bytes']<0 or not is_hash(row.get('sha256')):raise ValueError('Safe original evidence file inventory required')
    for row in rows:
        caseid=row['case']
        for field in ['prepared','verified']:
            if files.get(caseid+'-'+field+'.json',{}).get('sha256')!=json_digest(row[field]):raise ValueError('Original native phase evidence inventory differs')
        for i,boot in enumerate(row['boots'],1):
            if boot['native']['buildID']!=receipt['runtimeBuildID'] or boot['native']['version']!=receipt['runtimeVersion']:raise ValueError('Native boot differs from locked Runtime context')
            name=caseid+'-'+str(i)+'-clean-session.jsonlz4';saved=boot['cleanSessionFile']
            if saved.get('evidenceFile')!=name or files.get(name)!={'bytes':saved['bytes'],'sha256':saved['sha256']}:raise ValueError('Original clean session inventory differs')
            for name in [caseid+f'-boot{i}-control.json',caseid+f'-boot{i}-protocol.log']:
                if name not in files or files[name]['bytes']==0:raise ValueError('Missing original control/protocol evidence')
            if files[caseid+f'-boot{i}-control.json']['sha256']!=json_digest(control_evidence(boot['control'])):raise ValueError('Original bootstrap control/pref evidence inventory differs')
    if not all(name in files and files[name]['bytes']>0 for name in ['feles.log','workspace-exit-boot1.log','normal-boot2.log','workspace-exit-boot2.log']):raise ValueError('Original native launch logs missing')
    if 'rpcFaultProbeRequested' in receipt or 'rpcFaultProbe' in receipt:
        if receipt.get('rpcFaultProbeRequested') is not True:raise ValueError('Native fault proof cannot be unrequested or skipped')
        validate_probe(receipt['rpcFaultProbe'],receipt,validate_isolation)
        if files.get('rpc-fault-probe-receipt.json',{}).get('sha256')!=json_digest(receipt['rpcFaultProbe']):raise ValueError('Separate native fault receipt inventory differs')
    if receipt.get('originalError') is not None or receipt.get('cleanupError') is not None or receipt.get('rpcClientCloseError') or receipt.get('ownedCleanupComplete') is not True:raise ValueError('Original failure/cleanup cannot become a pass')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--rpc-failure-probe',action='store_true');args=parser.parse_args()
    out=ROOT/'_dist/workspace-restart-development';out.mkdir(mode=0o700)
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    if os.environ.get('GITHUB_RUN_ATTEMPT')!='1' or head!=os.environ.get('GITHUB_SHA'):raise ValueError('Original source/attempt required')
    lock=json.loads((ROOT/'floorp-runtime.lock.json').read_text());artifact=next(a for a in lock['artifacts'] if a['platform']=='linux' and a['architecture']=='x86_64')
    binary=ROOT/'_dist/bin/floorp/floorp-bin';ini=configparser.ConfigParser();ini.read(binary.parent/'application.ini')
    if ini['App']['Version']!=artifact['version'] or ini['App']['BuildID']!=artifact['buildId']:raise ValueError('Locked Runtime version/BuildID differs')
    hashes={p:digest(ROOT/p) for p in SOURCE_PATHS}
    receipt={'schema':'qa3-workspace-restart-development-v1','scope':'ORDINARY_DEVELOPMENT_E2E','status':'FAIL_DEVELOPMENT_ONLY','floorpHead':head,'run':int(os.environ['GITHUB_RUN_ID']),'attempt':1,'expectedCaseCount':2,'runtimeSource':lock['source']['commit'],'runtimeArtifact':artifact['asset'],'runtimeBuildID':artifact['buildId'],'runtimeVersion':artifact['version'],'sourceHashes':hashes,'nativeProductQualification':'UNVERIFIED','compileProfileQualification':'UNVERIFIED','publicationAuthorized':False,'nativeForcedCleanup':False,'cases':[],'originalError':None,'cleanupError':None}
    profile=ROOT/'_dist/profile/test'
    if profile.exists() or profile.is_symlink():raise ValueError('Synthetic profile must be created once by this controller')
    home=out/'home';home.mkdir(mode=0o700)
    env={k:os.environ[k] for k in ['PATH','LANG','DENO_DIR','CI','GITHUB_ACTIONS'] if k in os.environ}
    env.update(HOME=str(home),TMPDIR=str(home),DISPLAY=':99',DENO_DIR=os.environ.get('DENO_DIR',str(Path.home()/'.cache/deno')))
    owned={};processes=[];selector=selectors.DefaultSelector();logs={};native_track=None;native_root=None;clients=[]
    deadline=time.monotonic()+1200;cleaning=False;cancellation=None;probe_receipt=None;probe_out=None;probe_output_created=False
    def cancel(sig,_):
        nonlocal cancellation
        cancellation=cancellation or ('Original restart controller cancelled '+str(sig))
        if not cleaning:raise InterruptedError(cancellation)
    old_signals={s:signal.signal(s,cancel) for s in [signal.SIGTERM,signal.SIGINT]}
    def capture():
        t=legacy.process_table();legacy.adopt_owned(t,owned)
        if native_track is not None:
            capture_browser(t,native_track,native_root)
            merge_native_ownership(owned,native_track)
        return t
    def pump():
        capture()
        if time.monotonic()>=deadline:raise TimeoutError('Original complete restart wall deadline')
        if not cleaning and feles is not None and feles.poll() is not None:raise ValueError('Original development server exited: '+str(feles.returncode))
        for key,_ in selector.select(0.05):
            data=os.read(key.fileobj.fileno(),65536)
            if not data:selector.unregister(key.fileobj);key.fileobj.close();continue
            name,f=key.data
            if f.tell()+len(data)>LIMIT:raise ValueError('Original restart log overflow '+name)
            f.write(data);f.flush()
    def spawn(name,args):
        proc=subprocess.Popen(args,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
        processes.append(proc);t=legacy.process_table();owned[proc.pid]=t[proc.pid]['start']
        f=(out/(name+'.log')).open('xb');logs[name]=f;selector.register(proc.stdout,selectors.EVENT_READ,(name,f));return proc
    def wait(predicate,seconds,message):
        end=time.monotonic()+seconds
        while not predicate():
            if time.monotonic()>=end:raise TimeoutError(message)
            pump()
    def new_profile(caseid):
        if profile.exists() or profile.is_symlink():raise ValueError('Previous synthetic profile survived case boundary')
        profile.mkdir(parents=True,mode=0o700);nonce=uuid.uuid4().hex
        write_json(profile/'qa3-restart-profile.json',{'nonce':nonce,'case':caseid,'head':head});os.chmod(profile/'qa3-restart-profile.json',0o600)
        (profile/'prefs.js').write_text('user_pref("floorp.workspaces.enabled", true);\nuser_pref("browser.startup.page", 3);\nuser_pref("marionette.port", 2828);\n')
        return nonce,profile_identity(profile)
    def boot(label,control,proc=None,record=None,scope_receipt=None,evidence_output=None):
        nonlocal native_track,native_root
        record=receipt['cases'][-1] if record is None else record
        scope_receipt=receipt if scope_receipt is None else scope_receipt
        evidence_output=out if evidence_output is None else evidence_output
        number=len(record['boots'])+1;selected={'root':None}
        def diagnostic_port_owner():
            rows=listeners()
            if not rows:return None
            try:return listener_owner(rows,capture(),owned,binary,profile)
            except ValueError:return None
        scope=RpcFailureScope(profile,evidence_output,record,scope_receipt,number,artifact,
              lambda:rpc_process_ownership(capture,owned,binary,profile,selected['root'],port_owner=diagnostic_port_owner))
        with scope.guard('listener-startup'):
            wait(lambda:bool(listeners()),180,'Original native listener startup')
            root=listener_owner(listeners(),capture(),owned,binary,profile);selected['root']=root
            native_root=root;native_track={root['pid']:root['start']};capture()
            if root['session']!=root['pid']:raise ValueError('Native browser must own an independent process session')
        def packet(data):
            if trace.tell()+len(data)+1>LIMIT:raise ValueError('Original protocol trace byte bound')
            trace.write(data+b'\n');trace.flush()
        with scope.guard('initialization'):
            trace=(evidence_output/(label+'-protocol.log')).open('xb');logs[label+'-protocol']=trace
            b={'root':root,'profileIdentity':profile_identity(profile),'binarySha256':digest(binary),'overlayBuildID2':(ROOT/'_dist/buildid2').read_text().strip(),'control':control,'exitCode':None,'normalExit':False,'proc':proc}
            record['boots'].append(b)
            client=Marionette(PORT,pump,packet,failure_sink=scope.note_rpc_failure);clients.append(client)
        client.failure_scope=scope
        return client,b
    def end_boot(client,b,caseid,first_feles=False,workspace_exit=False,quit_scheduled=False):
        nonlocal native_track,native_root
        if quit_scheduled:client.close()
        else:
            with client.failure_scope.guard('quit'):
                client.schedule_quit(workspace_exit);client.close()
        def gone():
            if b.get('proc') is not None:b['proc'].poll()
            return not present(capture(),native_track) and not listeners()
        wait(gone,35,'Original graceful shutdown/children/port deadline')
        code=None
        if first_feles:
            portfile=ROOT/'_dist/marionette-port.txt'
            wait(lambda:not portfile.exists() and '[launcher] Browser Closed' in (out/'feles.log').read_text(),10,'Original launcher reaped first browser')
            events=[json.loads(unquote(m[1])) for m in re.finditer(r'\[launcher@native-exit\] (%7B[^\s"]+)',(out/'feles.log').read_text())]
            if len(events)!=1 or events[0]['command'][0]!=str(binary) or str(profile) not in events[0]['command'] or events[0].get('ownLinuxSession') is not True or events[0].get('launchCommand')!=['setsid','--wait',*events[0]['command']]:raise ValueError('Original launcher exit identity/session differs')
            code=events[0]['exitCode'];b['launcherExit']=events[0]
        else:
            code=b.pop('proc').wait(timeout=5)
        if code!=0:raise ValueError('Original browser exit was not successful: '+str(code))
        b.pop('proc',None);b.update(normalExit=True,exitCode=code,oldTreeGone=True,portGone=True,exitedBrowserIdentities={str(pid):start for pid,start in native_track.items()})
        if quit_scheduled:
            native_track=None;native_root=None
            return None
        clean=profile/'sessionstore.jsonlz4'
        if clean.is_symlink() or not clean.is_file() or clean.stat().st_size>4*1024*1024:raise ValueError('Original clean session file missing/unsafe/oversized')
        path=out/(caseid+'-'+str(len(receipt['cases'][-1]['boots']))+'-clean-session.jsonlz4');shutil.copyfile(clean,path)
        decode(path.read_bytes())
        b['cleanSessionFile']={'bytes':path.stat().st_size,'sha256':digest(path),'evidenceFile':path.name}
        native_track=None;native_root=None
        return clean
    userjs=None;feles=None
    try:
        if listeners() or Path('/tmp/.X11-unix/X99').exists():raise ValueError('Native display/port must be fresh')
        spawn('xvfb',['Xvfb',':99','-screen','0','1280x1024x24'])
        wait(lambda:Path('/tmp/.X11-unix/X99').exists(),10,'Original display startup')
        for index,(caseid,name) in enumerate(CASES.items()):
            record={'case':caseid,'name':name,'index':index+1,'status':'RUNNING','boots':[],'firstError':None};receipt['cases'].append(record)
            nonce,pidentity=new_profile(caseid);record['nonce']=nonce;record['profileIdentity']=pidentity
            control,_=write_boot_control(profile,caseid,1,receipt['run'])
            write_json(out/(caseid+'-boot1-control.json'),control_evidence(control))
            if index==0:feles=spawn('feles',['deno','task','feles-build','test','--own-native-session'])
            else:
                (profile/'user.js').write_bytes(userjs)
                if feles.poll() is not None:raise ValueError('Original development server exited between cases')
                proc=spawn(caseid+'-boot1',[str(binary),'--profile',str(profile),'--marionette','--remote-allow-system-access','--no-remote'])
            client,first=boot(caseid+'-boot1',control,None if index==0 else proc)
            prepared=browser_phase(client,{'action':'prepare','case':caseid,'nonce':nonce,'control':control},first,artifact,profile,out,record,receipt)
            first['native']=validate_phase(prepared,'prepare',caseid,nonce,control,first,artifact);record['prepared']=prepared;write_json(out/(caseid+'-prepared.json'),prepared)
            if (profile/'nora-tests-control.json').exists():raise ValueError('Original native bootstrap did not consume its control')
            if index==0:userjs=(profile/'user.js').read_bytes()
            clean=end_boot(client,first,caseid,index==0,caseid=='workspace-exit')
            validate_saved(decode(clean.read_bytes()),prepared)
            record.update(cleanSessionSaved=True,oldBrowserTreeGoneBeforeSecondBoot=True,oldPortGoneBeforeSecondBoot=True)
            if profile_identity(profile)!=pidentity:raise ValueError('Synthetic profile changed before second boot')
            before=digest(clean);record['cleanSessionBeforeSecondBootSha256']=before
            control,_=write_boot_control(profile,caseid,2,receipt['run'])
            write_json(out/(caseid+'-boot2-control.json'),control_evidence(control))
            if digest(clean)!=before or profile_identity(profile)!=pidentity:raise ValueError('Bootstrap control altered original clean session/profile')
            second_proc=spawn(caseid+'-boot2',[str(binary),'--profile',str(profile),'--marionette','--remote-allow-system-access','--no-remote'])
            client,second=boot(caseid+'-boot2',control,second_proc)
            verified=browser_phase(client,{'action':'verify','case':caseid,'nonce':nonce,'expected':prepared,'control':control},second,artifact,profile,out,record,receipt)
            second['native']=validate_phase(verified,'verify',caseid,nonce,control,second,artifact);record['verified']=verified;write_json(out/(caseid+'-verified.json'),verified)
            if (profile/'nora-tests-control.json').exists():raise ValueError('Original second bootstrap did not consume its control')
            # The locked SessionWriter moves clean to previous on its first
            # write, which can be the final shutdown write after a fast restore.
            # Observe that native boundary; do not manufacture a session save.
            final_clean=end_boot(client,second,caseid)
            validate_saved(decode(final_clean.read_bytes()),prepared)
            previous=profile/'sessionstore-backups/previous.jsonlz4'
            if previous.is_symlink() or not previous.is_file() or digest(previous)!=before:raise ValueError('Second boot did not consume the original clean session bytes')
            record['consumedOriginalCleanSessionSha256']=digest(previous)
            shutil.rmtree(profile);record['syntheticProfileRemoved']=True
            record['status']='PASS_DEVELOPMENT_ONLY';valid_case(record)
        if len(receipt['cases'])!=2:raise ValueError('Exactly two original restart cases required')
        if args.rpc_failure_probe:
            receipt['rpcFaultProbeRequested']=True
            probe_record={'case':'normal','purpose':'NATIVE_RPC_FAULT_INDUCTION_ONLY','boots':[]}
            probe_receipt={'schema':'qa3-native-rpc-fault-probe-v1','scope':'NATIVE_RPC_FAULT_INDUCTION_ONLY','status':'RUNNING',
                **{k:receipt[k] for k in ['floorpHead','run','attempt','sourceHashes','runtimeVersion','runtimeBuildID','nativeProductQualification','compileProfileQualification','publicationAuthorized']},
                'injectionKind':'NATIVE_ASYNC_EXCEPTION_BEFORE_CALLBACK','expectedRejection':False,'productPassingCasesContributed':0,'record':probe_record,'forcedCleanup':False}
            receipt['rpcFaultProbe']=probe_receipt
            probe_out=out/'rpc-fault-probe';probe_out.mkdir(mode=0o700);probe_output_created=True
            nonce,pidentity=new_profile('normal');probe_record.update(nonce=nonce,profileIdentity=pidentity)
            control,_=write_boot_control(profile,'rpc-fault-probe',1,receipt['run'])
            write_json(probe_out/'rpc-fault-probe-boot1-control.json',control_evidence(control))
            (profile/'user.js').write_bytes(userjs)
            proc=spawn('rpc-fault-probe-boot1',[str(binary),'--profile',str(profile),'--marionette','--remote-allow-system-access','--no-remote'])
            client,probe_boot=boot('rpc-fault-probe-boot1',control,proc,probe_record,probe_receipt,probe_out)
            exercise_native_fault(client,probe_receipt,artifact,validate_isolation)
            end_boot(client,probe_boot,'rpc-fault-probe',quit_scheduled=True)
            probe_receipt['postFailureRpcCount']=client.index-probe_receipt['expectedFaultRequestId']-1
            if client.index!=probe_receipt['requestCounterAfterFault']:raise ValueError('RPC was performed after native fault')
            if profile_identity(profile)!=pidentity:raise ValueError('Native fault synthetic profile changed')
            shutil.rmtree(profile);probe_receipt['syntheticProfileRemoved']=True
            probe_receipt['status']=VERIFIED
        if any(digest(ROOT/path)!=value for path,value in hashes.items()):raise ValueError('Source changed during original restart')
        receipt['status']='PASS_DEVELOPMENT_ONLY'
    except BaseException as error:
        observation=(probe_receipt if probe_receipt is not None else receipt).pop('_rpcFailureRecord',None)
        receipt['originalError']=failure_text(error,observation)
        if probe_receipt is not None:
            probe_receipt['status']='FAULT_PROBE_FAILED';probe_receipt['originalError']=receipt['originalError']
        elif receipt['cases']:
            receipt['cases'][-1]['status']='FAIL_DEVELOPMENT_ONLY';receipt['cases'][-1]['firstError']=receipt['originalError']
        for caseid,name in CASES.items():
            if not any(r['case']==caseid for r in receipt['cases']):receipt['cases'].append({'case':caseid,'name':name,'status':'UNEXECUTED'})
    finally:
        cleaning=True
        for client in clients:
            try:client.close()
            except BaseException as error:
                receipt['rpcClientCloseError']='RPC client close failed: '+exception_type(error)
                receipt['status']='FAIL_DEVELOPMENT_ONLY'
        try:
            t=capture()
            def reject_forced_native(table):
                for pid in live(table,owned):
                    try:exe=Path(os.readlink(Path('/proc')/str(pid)/'exe')).resolve()
                    except (FileNotFoundError,ProcessLookupError):continue
                    if exe.is_relative_to(binary.parent.resolve()):
                        receipt['nativeForcedCleanup']=True;receipt['originalError']=receipt['originalError'] or ('Owned native process needs forced final cleanup: '+str(pid));receipt['status']='FAIL_DEVELOPMENT_ONLY'
            for sig in [signal.SIGTERM,signal.SIGKILL]:
                reject_forced_native(t)
                for pid in live(t,owned):
                    try:signal_owned(pid,owned[pid],sig)
                    except ProcessLookupError:pass
                end=time.monotonic()+(5 if sig==signal.SIGTERM else 2)
                while time.monotonic()<end:
                    for proc in processes:proc.poll()
                    time.sleep(0.05)
                t=capture()
            if live(t,owned) or listeners():raise ValueError('Owned processes/port survived final cleanup')
            receipt['ownedCleanupComplete']=True
        except BaseException as error:receipt['cleanupError']=str(error);receipt['status']='FAIL_DEVELOPMENT_ONLY'
        if profile.exists() and receipt.get('ownedCleanupComplete'):
            try:shutil.rmtree(profile)
            except BaseException as error:receipt['cleanupError']=receipt['cleanupError'] or str(error);receipt['status']='FAIL_DEVELOPMENT_ONLY'
        for record in receipt['cases']+([] if probe_receipt is None else [probe_receipt['record']]):
            for b in record.get('boots',[]):b.pop('proc',None)
        for f in logs.values():f.close()
        if probe_receipt is not None:
            probe_receipt['forcedCleanup']=receipt['nativeForcedCleanup']
            probe_receipt['evidenceFiles']={p.name:{'bytes':p.stat().st_size,'sha256':digest(p)} for p in probe_out.iterdir() if p.is_file()} if probe_output_created else {}
            if probe_receipt['status']==VERIFIED:
                try:validate_probe(probe_receipt,receipt,validate_isolation)
                except BaseException as error:
                    probe_receipt['status']='FAULT_PROBE_FAILED';probe_receipt['verificationError']='Native fault closure failed: '+exception_type(error)
                    receipt['originalError']=receipt['originalError'] or probe_receipt['verificationError'];receipt['status']='FAIL_DEVELOPMENT_ONLY'
            write_json(out/'rpc-fault-probe-receipt.json',probe_receipt)
        for path in out.glob('*.log'):receipt.setdefault('logs',{})[path.name]={'bytes':path.stat().st_size,'sha256':digest(path)}
        receipt['evidenceFiles']={p.name:{'bytes':p.stat().st_size,'sha256':digest(p)} for p in out.iterdir() if p.is_file() and p.suffix in ['.json','.log','.jsonlz4'] and p.name!='receipt.json'}
        if cancellation:receipt['originalError']=receipt['originalError'] or cancellation;receipt['status']='FAIL_DEVELOPMENT_ONLY'
        if receipt['status']=='PASS_DEVELOPMENT_ONLY':
            try:validate_receipt(receipt)
            except BaseException as error:receipt['originalError']=str(error);receipt['status']='FAIL_DEVELOPMENT_ONLY'
        write_json(out/'receipt.json',receipt)
        for s,handler in old_signals.items():signal.signal(s,handler)
    print(json.dumps(receipt))
    if receipt['status']!='PASS_DEVELOPMENT_ONLY':raise ValueError('Original restart failed; first failure/evidence retained')

if __name__=='__main__':main()
