# SPDX-License-Identifier: MPL-2.0
"""Short, bounded CPU/RSS/process-churn experiment for the native monitor."""
import argparse
import hashlib
import json
import selectors
import subprocess
import sys
import time
from pathlib import Path

from support import Failure,require,write_json

SECONDS=60
WORKERS=3
WORKER_MIB=1536


def worker(seconds,mib):
    # Touch each page before the ready message; allocation alone does not prove RSS.
    memory=bytearray(mib*1024**2)
    for offset in range(0,len(memory),4096):memory[offset]=1
    print('ready',flush=True)
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        hashlib.sha256(memoryview(memory)[:4*1024**2]).digest()


def cleanup_children(children):
    records=[]
    for index,child in enumerate(children):
        row={'index':index,'reaped':False,'failure':None}
        records.append(row);operation='poll'
        try:
            if child.poll() is None:
                operation='terminate';child.terminate()
            operation='wait'
            try:child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                operation='kill';child.kill();operation='wait-after-kill';child.wait(timeout=5)
            row['reaped']=child.returncode is not None
        except BaseException as error:
            kind=next((name for cls,name in ((PermissionError,'permission'),
                (subprocess.TimeoutExpired,'timeout'),(ProcessLookupError,'process-lookup'),
                (OSError,'os-error'),(KeyboardInterrupt,'interrupted')) if isinstance(error,cls)),'exception')
            row['failure']={'operation':operation,'kind':kind}
        finally:
            try:
                if child.stdout is not None and not child.stdout.closed:child.stdout.close()
            except BaseException:
                row['streamCloseFailed']=True
    return records


def workload(receipt_path=None):
    workers=[];churn=[];spawned=reaped=0
    receipt={'status':'failed','workers':WORKERS,'workerMiB':WORKER_MIB}
    began=time.monotonic()
    try:
        for _ in range(WORKERS):
            child=subprocess.Popen([sys.executable,__file__,'--worker','--seconds',str(SECONDS),
                                    '--mib',str(WORKER_MIB)],stdout=subprocess.PIPE,text=True)
            workers.append(child)
        ready_deadline=time.monotonic()+15
        with selectors.DefaultSelector() as selector:
            for child in workers:selector.register(child.stdout,selectors.EVENT_READ)
            while selector.get_map():
                events=selector.select(max(0,ready_deadline-time.monotonic()))
                require(events and time.monotonic()<ready_deadline,'inventory-stress-ready-timeout')
                for key,_ in events:
                    require(key.fileobj.readline().strip()=='ready','inventory-stress-ready')
                    selector.unregister(key.fileobj)
        for child in workers:child.stdout.close()
        until=time.monotonic()+SECONDS
        while time.monotonic()<until:
            for _ in range(4):
                churn.append(subprocess.Popen([sys.executable,'-c','import time;time.sleep(0.03)'],
                                             stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
            spawned+=len(churn)
            for child in churn:
                require(child.wait(timeout=5)==0,'inventory-stress-churn-exit');reaped+=1
            churn=[]
        for child in workers:require(child.wait(timeout=10)==0,'inventory-stress-worker-exit')
        receipt.update(status='passed',churnChildren=spawned,churnReaped=reaped,
                       workersReaped=len(workers),seconds=round(time.monotonic()-began,3))
    finally:
        # Only exact children created above receive signals. The outer Runner also
        # verifies this dedicated process group has no survivor and reaps its leader.
        primary_pending=sys.exc_info()[0] is not None
        receipt['cleanup']=cleanup_children([*churn,*workers])
        failed=any(not row['reaped'] or row['failure'] is not None or row.get('streamCloseFailed')
                   for row in receipt['cleanup'])
        if failed:receipt['status']='failed'
        try:
            if receipt_path is not None:write_json(receipt_path,receipt)
            print(json.dumps({'inventoryStressWorkload':receipt}),flush=True)
        except BaseException:
            if not primary_pending:raise
        if failed and not primary_pending:raise Failure('inventory-stress-child-cleanup')


def run_experiment(task,runner,env,receipt):
    receipt.update(status='collecting',seconds=SECONDS,workers=WORKERS,workerMiB=WORKER_MIB,
                   samples=0,completeSamples=0,retriedSamples=0,unknownSamples=0,
                   timeoutAttempts=0,peakRSSMiB=None,peakCPUSeconds=None,
                   maxSampleSeconds=0,backends={})
    original=runner.group_activity
    had_override='group_activity' in runner.__dict__
    def observe(pgid):
        began=time.monotonic()
        value=original(pgid)
        receipt['samples']+=1
        receipt['maxSampleSeconds']=round(max(receipt['maxSampleSeconds'],time.monotonic()-began),4)
        backend=value.get('inventoryBackend','unavailable')
        receipt['backends'][backend]=receipt['backends'].get(backend,0)+1
        receipt['completeSamples']+=int(value['sampleComplete'])
        receipt['unknownSamples']+=int(not value['sampleComplete'])
        receipt['retriedSamples']+=int(value['sampleAttempts']>1)
        receipt['timeoutAttempts']+=sum(row['unknownKinds'].get('inventory-timeout',0)
                                        for row in value['partialSamples'])
        for source,target in (('rssMiB','peakRSSMiB'),('cpuSeconds','peakCPUSeconds')):
            if value[source] is not None:receipt[target]=max(receipt[target] or 0,value[source])
        return value
    runner.group_activity=observe
    try:
        path=task/'inventory-stress-workload.json'
        runner.run('inventory-stress',[sys.executable,str(Path(__file__).resolve()),'--receipt',str(path)],
                   task,env,360,phase_deadline_minutes=15)
        value=json.loads(path.read_text())
        require(value.get('status')=='passed' and value.get('workersReaped')==WORKERS and
                type(value.get('churnChildren')) is int and value['churnChildren']>0 and
                value.get('churnReaped')==value['churnChildren'] and value.get('seconds',0)>=SECONDS,
                'inventory-stress-workload')
        receipt['workload']=value
        require(receipt['samples']>=20 and receipt['completeSamples']==receipt['samples'] and
                receipt['backends']=={'kern-proc-pgrp-helper':receipt['samples']} and
                receipt['timeoutAttempts']==0 and receipt['peakRSSMiB'] is not None and
                receipt['peakRSSMiB']>=2048 and receipt['peakCPUSeconds'] is not None and
                receipt['peakCPUSeconds']>=20,'inventory-stress-monitor')
        record=runner.records[-1]
        require(record['groupCleanup']=='verified-gone' and record['leaderReap']=={'status':'reaped','exitCode':0}
                and record['cleanupFailure'] is None,'inventory-stress-cleanup')
        receipt['status']='passed'
    except BaseException:
        receipt['status']='failed'
        raise
    finally:
        try:
            path=task/'inventory-stress-workload.json'
            if path.is_file():receipt['workload']=json.loads(path.read_text())
        except (OSError,ValueError):receipt['workloadReceiptUnavailable']=True
        finally:
            # Restore the original method even when evidence collection fails.
            if had_override:runner.group_activity=original
            else:del runner.group_activity


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--worker',action='store_true')
    parser.add_argument('--seconds',type=float,default=SECONDS)
    parser.add_argument('--mib',type=int,default=WORKER_MIB)
    parser.add_argument('--receipt',type=Path)
    args=parser.parse_args()
    require(0<args.seconds<=SECONDS and 0<args.mib<=WORKER_MIB,'inventory-stress-arguments')
    if args.worker:worker(args.seconds,args.mib)
    else:workload(args.receipt)


if __name__=='__main__':main()
