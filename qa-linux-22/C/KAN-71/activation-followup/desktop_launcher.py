#!/usr/bin/python3
import os,sys,re,json,time
from pathlib import Path
out,exe,profile=sys.argv[1:4]
sid=os.environ.get('DESKTOP_STARTUP_ID','');match=re.search(r'_TIME(\d+)',sid)
with Path(out).open('a') as f:f.write(json.dumps({'monotonic':time.monotonic(),'startupIdPresent':bool(sid),'startupTimestamp':int(match.group(1)) if match else None,'activationTokenPresent':bool(os.environ.get('XDG_ACTIVATION_TOKEN'))})+'\n')
os.execv(exe,[exe,'--profile',profile,*sys.argv[4:]])
