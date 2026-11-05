#!/usr/bin/env python3
"""One owned fresh-profile diagnostic on the actual desktop; never fabricates GPU/backend.
Not executed against the GPU-less Cloud host. Keep full traces local for review.
"""
import argparse,hashlib,json,os,signal,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--protocol',choices=['wayland','x11'],required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--capture-stack',action='store_true');a=p.parse_args()
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_GPU_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX','MOZ_DISABLE_SOCKET_PROCESS_SANDBOX']:
 if os.environ.get(k):raise SystemExit('Refusing inherited protection-disabling variable: '+k)
b=a.binary.resolve(strict=True);o=a.out.resolve();o.mkdir(parents=True,exist_ok=False);profile=o/'profile';profile.mkdir();home=o/'home';home.mkdir();env=dict(os.environ)
env.update(HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_DATA_HOME=str(home/'data'),XDG_CACHE_HOME=str(home/'cache'),MOZ_ENABLE_WAYLAND='1' if a.protocol=='wayland' else '0',WAYLAND_DEBUG='client',MOZ_APP_REMOTINGNAME='qa-kan7-'+str(os.getpid()))
# Real desktop DISPLAY/WAYLAND_DISPLAY/XDG_RUNTIME_DIR and DBus are inherited, never invented.
# No user.js, forced renderer, portal override, sandbox or TLS change.
res={'binarySha256':hashlib.sha256(b.read_bytes()).hexdigest(),'requestedProtocol':a.protocol,'protocolActuallyObserved':None,'note':'Requested backend alone is not proof of actual window protocol; inspect trace/about:support. No GPU equivalence claim.','command':['<verified-product>','--no-remote','--profile','<fresh-owned-profile>','about:blank']}
for n in ['application.ini','platform.ini']:
 q=b.parent/n
 if q.exists():res[n]=q.read_text()
log=open(o/'startup.log','w');proc=subprocess.Popen([str(b),'--no-remote','--profile',str(profile),'about:blank'],env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True);res['parentPid']=proc.pid
try:
 time.sleep(30);res['aliveAfter30s']=proc.poll() is None;res['returnCodeBeforeCleanup']=proc.poll();res['threads']=[]
 if proc.poll() is None:
  for d in (Path('/proc')/str(proc.pid)/'task').iterdir():
   try:res['threads'].append({'tid':int(d.name),'comm':(d/'comm').read_text().strip(),'wchan':(d/'wchan').read_text().strip()})
   except OSError:pass
  if a.capture_stack:
   try:
    q=subprocess.run(['gdb','--batch','-ex','set debuginfod enabled off','-ex','set pagination off','-ex','thread apply all bt','-p',str(proc.pid)],capture_output=True,text=True,timeout=20);(o/'owned-parent-stack.txt').write_text(q.stdout+q.stderr);res['gdbReturnCode']=q.returncode
   except (OSError,subprocess.TimeoutExpired) as e:res['gdbError']=type(e).__name__
finally:
 if proc.poll() is None:
  os.killpg(proc.pid,signal.SIGTERM)
  try:proc.wait(timeout=5)
  except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
 log.close();res['returnCodeAfterOwnedCleanup']=proc.returncode;(o/'observation.json').write_text(json.dumps(res,indent=2)+'\n')
print('Saved local owned-process diagnostics. Review traces before external export; process survival is not successful startup.')
