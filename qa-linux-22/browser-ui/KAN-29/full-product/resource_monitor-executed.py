"""Passive cgroup/owned process monitor; no product/pref/render changes."""
import os,time,json,threading,signal
from pathlib import Path
class ResourceBoundExceeded(RuntimeError):pass
class Monitor:
 def __init__(self,path,child_getter,own_getter):
  self.path=Path(path);self.child_getter=child_getter;self.own_getter=own_getter;self.stop_event=threading.Event();self.exceeded=None;self.worker=os.getpid();self.baseline=self.sample();self.baseline['kind']='before-any-browser';self.write(self.baseline);self.thread=threading.Thread(target=self.run,daemon=True)
 def write(self,d):
  with self.path.open('a') as f:f.write(json.dumps(d)+'\n')
 def sample(self):
  rows={}
  for d in Path('/proc').iterdir():
   if not d.name.isdigit():continue
   try:
    fields={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in (d/'status').read_text().splitlines() if ':' in line};rows[int(d.name)]={'pid':int(d.name),'ppid':int(fields['PPid']),'name':fields['Name'],'rssKiB':int(fields.get('VmRSS','0 kB').split()[0])}
   except (OSError,ValueError,KeyError):pass
  root=self.child_getter();ids={root} if root else set();changed=True
  while changed:
   added={pid for pid,row in rows.items() if row['ppid'] in ids}-ids;changed=bool(added);ids|=added
  cg=Path('/sys/fs/cgroup');current=int((cg/'memory.current').read_text());maximum=(cg/'memory.max').read_text().strip();events=dict((line.split()[0],int(line.split()[1])) for line in (cg/'memory.events').read_text().splitlines());owned=[rows[i] for i in sorted(ids) if i in rows]
  for row in owned:
   try:
    rollup=Path('/proc')/str(row['pid'])/'smaps_rollup';values={line.split(':',1)[0]:line.split(':',1)[1].strip() for line in rollup.read_text().splitlines() if ':' in line};row['pssKiB']=int(values['Pss'].split()[0])
   except (OSError,ValueError,KeyError):row['pssKiB']=None
  return {'monotonic':time.monotonic(),'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'cgroupBytes':current,'cgroupMax':maximum,'cgroupEvents':events,'actualFloorpChildPid':root,'floorpTree':owned,'aggregateFloorpRssKiB':sum(x['rssKiB'] for x in owned),'aggregateFloorpKnownPssKiB':sum(x['pssKiB'] or 0 for x in owned),'allOwnedPssAvailable':all(x['pssKiB'] is not None for x in owned),'rssWarningOnly':sum(x['rssKiB'] for x in owned)>8*1024*1024,'otherOwnedProcesses':[rows[i] for i in self.own_getter() if i in rows],'note':'Aggregate RSS may count shared pages more than once; cgroup includes other tasks. OOM counters alone do not identify victim/cause.'}
 def start(self):self.thread.start()
 def run(self):
  while not self.stop_event.wait(2):
   try:
    d=self.sample();self.write(d);maximum=int(d['cgroupMax']) if d['cgroupMax'].isdigit() else 0
    if d['actualFloorpChildPid'] and (d['aggregateFloorpKnownPssKiB']>12*1024*1024 or maximum and d['cgroupBytes']>maximum*.90):
     self.exceeded={'observation':d,'bound':'QA safety bound: measured owned Floorp known PSS12GiB or cgroup90%; RSS8GiB warning only. Not proof of original host inadequacy/product cause; never PASS'};self.write({'kind':'resource-bound','detail':self.exceeded});self.stop_event.set();os.kill(self.worker,signal.SIGUSR1);return
   except Exception as e:self.write({'kind':'monitor-error','error':repr(e)})
 def stop(self):
  self.stop_event.set();self.thread.join(timeout=3)
  try:self.write(dict(self.sample(),kind='after-owned-products'))
  except Exception as e:self.write({'kind':'final-monitor-error','error':repr(e)})
