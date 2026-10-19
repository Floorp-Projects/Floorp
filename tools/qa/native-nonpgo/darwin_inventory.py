# SPDX-License-Identifier: MPL-2.0
"""Bounded-child primary group inventory using header-bound KERN_PROC_PGRP.

No alternate API after failure. The caller owns the 3s subprocess timeout/reap.
No raw inventory or exception text is published by the parent.
"""
import sys
_PROGRESS_SEQUENCE=0
if __name__=='__main__':
    # Distinguish reaching the script from loading the project and ctypes APIs.
    print('{"inventoryProgress":"project-imports","sequence":1}',flush=True)
    _PROGRESS_SEQUENCE=1

import argparse
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import re

from process_census import CAP,canonical,decode,load_abi,validate_layout
from qa_core import Failure,require

PHASES=('project-imports','abi-binding','reader-binding','self-bsd','self-snapshot','self-size','self-data',
        'group-snapshot','group-size','group-data','group-decode')


def progress_reply(output):
    """Read fixed progress only; incomplete inventory never becomes a measurement."""
    if isinstance(output,bytes):
        if len(output)>65536:return {'phase':'unobserved','events':0}
        try:output=output.decode('utf-8')
        except UnicodeError:return {'phase':'unobserved','events':0}
    if not isinstance(output,str):return {'phase':'unobserved','events':0}
    try:size=len(output.encode())
    except UnicodeError:return {'phase':'unobserved','events':0}
    if size>65536:
        return {'phase':'unobserved','events':0}
    phases=[]
    for line in output.splitlines(keepends=True):
        if not line.endswith('\n'):break
        try:value=json.loads(line)
        except (ValueError,RecursionError):break
        if (not isinstance(value,dict) or set(value)!={'inventoryProgress','sequence'} or
                value['inventoryProgress'] not in PHASES or type(value['sequence']) is not int or
                value['sequence']!=len(phases)+1):
            break
        phases.append(value['inventoryProgress'])
        if len(phases)>32:return {'phase':'unobserved','events':0}
    return {'phase':phases[-1] if phases else 'unobserved','events':len(phases)}


def read_records(layout,selector,subject,queries,*,allow_empty=False,system=None,progress=None):
    validate_layout(layout)
    require(type(subject) is int and 0<subject<2**31 and type(selector) is int and
            selector in (1,2),'inventory-selector')
    system=system or ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True)
    system.sysctl.argtypes=[ctypes.POINTER(ctypes.c_int),ctypes.c_uint,ctypes.c_void_p,
                           ctypes.POINTER(ctypes.c_size_t),ctypes.c_void_p,ctypes.c_size_t]
    system.sysctl.restype=ctypes.c_int
    mib=(ctypes.c_int*4)(1,14,selector,subject)
    for attempt in range(3):
        needed=ctypes.c_size_t()
        if progress:progress('self-size' if selector==1 else 'group-size')
        ctypes.set_errno(0)
        code=system.sysctl(mib,4,None,ctypes.byref(needed),None,0)
        error=ctypes.get_errno()
        queries.append({'query':'self-size' if selector==1 else 'group-size',
                        'attempt':attempt+1,'returnCode':code,'errno':error,'bytes':needed.value})
        if code!=0 or error:
            if error==errno.ENOMEM and attempt<2:continue
            raise Failure('inventory-query-permission' if error in (errno.EPERM,errno.EACCES)
                          else 'inventory-size-unavailable')
        require(0<=needed.value<=CAP and needed.value%layout['recordBytes']==0 and
                (allow_empty or needed.value>0),'inventory-size')
        capacity=min(CAP,needed.value+32*layout['recordBytes'])
        data=ctypes.create_string_buffer(capacity)
        length=ctypes.c_size_t(capacity)
        if progress:progress('self-data' if selector==1 else 'group-data')
        ctypes.set_errno(0)
        code=system.sysctl(mib,4,data,ctypes.byref(length),None,0)
        error=ctypes.get_errno()
        queries.append({'query':'self-data' if selector==1 else 'group-data',
                        'attempt':attempt+1,'returnCode':code,'errno':error,
                        'bytes':length.value,'capacityBytes':capacity})
        if code!=0 or error:
            if error==errno.ENOMEM and attempt<2:continue
            raise Failure('inventory-query-permission' if error in (errno.EPERM,errno.EACCES)
                          else 'inventory-data-unavailable')
        require(0<=length.value<=capacity and length.value%layout['recordBytes']==0 and
                (allow_empty or length.value>0),'inventory-returned-size')
        return data.raw[:length.value]
    raise Failure('inventory-retries-exhausted')


def pids_from_records(data,layout):
    size=layout['recordBytes']
    require(len(data)<=CAP and len(data)%size==0,'inventory-record-length')
    pids=set()
    for offset in range(0,len(data),size):
        row={name:int.from_bytes(data[offset+field['offset']:offset+field['offset']+field['bytes']],
                                 'little',signed=field['signed']) for name,field in layout['fields'].items()}
        require(0<row['pid']<2**31 and row['pid'] not in pids,'inventory-record-pid')
        require(row['startSec']>0 and 0<=row['startUsec']<1000000 and row['state'] in (1,2,3,4,5) and
                all(0<=row[name]<2**32-1 for name in ('uid','ruid')),'inventory-record-identity')
        # Every UID and valid zombie remains in inventory for the existing BSD/task checks.
        pids.add(row['pid'])
    return sorted(pids)


def collect(pgid,queries,*,layout=None,reader=None,system=None,progress=None,notify=None):
    require(type(pgid) is int and 0<pgid<2**31,'inventory-group-id')
    progress={} if progress is None else progress
    def mark(phase):
        progress['phase']=phase
        if notify:notify(phase)
    mark('abi-binding')
    layout=layout or load_abi(Path(os.environ['FLOORP_CENSUS_ABI']))
    if reader is None:
        mark('reader-binding')
        from darwin_activity import probe
        reader=probe()
    mark('self-bsd')
    state,own,_=reader.bsd(os.getpid())
    require(state=='measured' and own is not None and own.pid==os.getpid() and
            own.uid==os.geteuid() and own.ruid==os.getuid(),'inventory-self-unavailable')
    mark('self-snapshot')
    data=read_records(layout,1,os.getpid(),queries,system=system,progress=mark)
    bound=decode(data,layout,os.geteuid(),os.getuid(),os.getpid(),(own.start_sec,own.start_usec))
    require(bound['measurement']['snapshotEntries']==1,'inventory-self-count')
    mark('group-snapshot')
    data=read_records(layout,2,pgid,queries,allow_empty=True,system=system,progress=mark)
    mark('group-decode')
    pids=pids_from_records(data,layout)
    return {'status':'measured','pids':pids,'group':pgid,
            'measurement':{'api':'sysctl-kern-proc-pgrp','snapshotBytes':len(data),
                'snapshotEntries':len(pids),'selfBindingVerified':True,
                'layoutSHA256':hashlib.sha256(canonical(layout)).hexdigest()},'queries':queries}


def decode_reply(text,pgid):
    """Strict private protocol; only its safe measurement/query fields reach reports."""
    require(isinstance(text,str) and len(text.encode())<=1024**2,'inventory-protocol-size')
    lines=text.splitlines()
    require(bool(lines),'inventory-protocol')
    if len(lines)>1:
        prefix='\n'.join(lines[:-1])+'\n'
        observed=progress_reply(prefix)
        require(observed['events']==len(lines)-1 and observed['events']>0,'inventory-protocol-progress')
    value=json.loads(lines[-1])
    require(isinstance(value,dict) and type(value.get('group')) is int and value.get('group')==pgid and
            value.get('status') in ('measured','unavailable'),'inventory-protocol')
    require(set(value)<= {'status','pids','group','measurement','queries','failureCode','errorType','errno','phase'},
            'inventory-protocol-keys')
    queries=value.get('queries')
    require(isinstance(queries,list) and len(queries)<=12,'inventory-protocol-queries')
    for row in queries:
        require(isinstance(row,dict) and set(row)<={'query','attempt','returnCode','errno','bytes','capacityBytes'} and
                row.get('query') in ('self-size','self-data','group-size','group-data') and
                all(type(item) is int for key,item in row.items() if key!='query'),'inventory-protocol-query')
    if value['status']=='unavailable':
        require(value.get('pids') is None,'inventory-protocol-unknown')
        require(value.get('phase') in ('startup',*PHASES),'inventory-protocol-failure')
        require(value.get('failureCode') in {'inventory-helper-exception','inventory-query-permission',
                'inventory-size-unavailable','inventory-data-unavailable','inventory-size',
                'inventory-returned-size','inventory-record-length','inventory-record-pid',
                'inventory-record-identity','inventory-self-unavailable','inventory-self-count',
                'inventory-abi-unavailable','inventory-group-id','inventory-selector',
                'inventory-retries-exhausted','inventory-platform'},'inventory-protocol-failure')
        require(value.get('errorType') in ('failure','os-error','exception') and
                ('errno' not in value or type(value['errno']) is int),'inventory-protocol-failure')
        return value
    pids=value.get('pids');measure=value.get('measurement')
    require(isinstance(pids,list) and len(pids)<=CAP//256 and
            all(type(pid) is int and 0<pid<2**31 for pid in pids) and len(set(pids))==len(pids),
            'inventory-protocol-pids')
    require(isinstance(measure,dict) and set(measure)=={'api','snapshotBytes','snapshotEntries',
                'selfBindingVerified','layoutSHA256'} and measure['api']=='sysctl-kern-proc-pgrp' and
            type(measure['snapshotBytes']) is int and 0<=measure['snapshotBytes']<=CAP and
            type(measure['snapshotEntries']) is int and measure['snapshotEntries']==len(pids) and
            measure['selfBindingVerified'] is True and
            re.fullmatch('[0-9a-f]{64}',measure['layoutSHA256'] or '') is not None,'inventory-protocol-measurement')
    return value


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--pgid',type=int,required=True)
    args=parser.parse_args();queries=[];progress={};sequence=_PROGRESS_SEQUENCE
    def emit(phase):
        nonlocal sequence
        sequence+=1
        print(json.dumps({'inventoryProgress':phase,'sequence':sequence}),flush=True)
    try:
        require(sys.platform=='darwin','inventory-platform')
        value=collect(args.pgid,queries,progress=progress,notify=emit)
    except Exception as error:
        code=str(error) if isinstance(error,Failure) else 'inventory-helper-exception'
        if code.startswith('census-'):code='inventory-abi-unavailable'
        value={'status':'unavailable','pids':None,'group':args.pgid,'queries':queries,
               'phase':progress.get('phase','startup'),
               'failureCode':code,'errorType':'os-error' if isinstance(error,OSError) else
                    'failure' if isinstance(error,Failure) else 'exception'}
        if isinstance(error,OSError) and type(error.errno) is int:value['errno']=error.errno
    print(json.dumps(value),flush=True)


if __name__=='__main__':main()
