# SPDX-License-Identifier: MPL-2.0
"""Primary in-process, read-only KERN_PROC census. No permission fallback."""
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import stat
import sys

from support import Failure, digest, require, write_json

HERE = Path(__file__).resolve().parent
CAP = 8 * 1024**2
FIELDS = {'pid': (4, True), 'startSec': (8, True), 'startUsec': (None, True),
          'state': (1, True), 'uid': (4, False), 'ruid': (4, False)}
BINDING_KEYS = ('FLOORP_FIXTURE_SHA','GITHUB_RUN_ID','GITHUB_RUN_ATTEMPT','GITHUB_SHA','ImageVersion')


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def validate_layout(value):
    require(set(value) == {'schema','pointerBytes','recordBytes','byteOrder','mib','fields'} and
            value['schema'] == 1 and value['pointerBytes'] == 8 and value['byteOrder'] == 'little' and
            value['mib'] == [1,14,0,0] and type(value['recordBytes']) is int and
            256 <= value['recordBytes'] <= 4096 and set(value['fields']) == set(FIELDS), 'census-abi-schema')
    used = set()
    for name,(size,signed) in FIELDS.items():
        field = value['fields'][name]
        require(set(field) == {'offset','bytes','signed'} and type(field['offset']) is int and
                type(field['bytes']) is int and type(field['signed']) is bool and
                field['signed'] is signed and (field['bytes'] == size if size else field['bytes'] in (4,8)) and
                0 <= field['offset'] < field['offset']+field['bytes'] <= value['recordBytes'], 'census-abi-field')
        positions = set(range(field['offset'], field['offset']+field['bytes']))
        require(not positions & used, 'census-abi-overlap')
        used.update(positions)
    return value


def binding(task):
    return {**{key:os.environ[key] for key in BINDING_KEYS},
            'jobOriginSHA256':digest(task/'job-origin.json'), 'sourceSHA256':digest(HERE/'census_abi.c')}


def prepare_abi(task, runner, env):
    require(sys.platform == 'darwin' and sys.byteorder == 'little' and ctypes.sizeof(ctypes.c_void_p)==8,
            'census-platform')
    binary = task/'census-abi'
    runner.run('census-abi-compile', ['/usr/bin/cc','-std=c11','-O2','-Wall','-Wextra','-Werror',
               str(HERE/'census_abi.c'),'-o',str(binary)], task, env, 360, phase_deadline_minutes=15)
    log = runner.run('census-abi', [str(binary)], task, env, 360, phase_deadline_minutes=15)
    require(log.stat().st_size <= 8192, 'census-abi-output-size')
    layout = validate_layout(json.loads(log.read_text()))
    rows = [row for row in runner.records if row['stage'] in ('census-abi-compile','census-abi')]
    require(len(rows)==2 and runner.failure is None and runner.monitor_failure is None and
            all(row['exitCode']==0 and row['groupCleanup']=='verified-gone' and
                row['leaderReap']['status']=='reaped' and row['cleanupFailure'] is None for row in rows),
            'census-abi-stage-completion')
    binary.chmod(0o500)
    value = {'schema':1,'binding':binding(task),'binarySHA256':digest(binary),'layout':layout,
             'layoutSHA256':hashlib.sha256(canonical(layout)).hexdigest()}
    path = task/'census-abi.json'
    require(not path.exists() and not path.is_symlink(), 'census-abi-not-fresh')
    write_json(path,value)
    path.chmod(0o400)
    return path


def load_abi(path):
    path = Path(path)
    task = Path(os.environ['TASK_ROOT']).resolve()
    require(path == task/'census-abi.json' and not path.is_symlink(), 'census-abi-path')
    info = path.lstat()
    binary = task/'census-abi'
    require(stat.S_ISREG(info.st_mode) and info.st_uid==os.geteuid() and
            stat.S_IMODE(info.st_mode)==0o400 and info.st_size<=16384 and
            binary.is_file() and not binary.is_symlink() and binary.stat().st_uid==os.geteuid() and
            stat.S_IMODE(binary.stat().st_mode)==0o500, 'census-abi-file')
    value = json.loads(path.read_text())
    require(set(value)=={'schema','binding','binarySHA256','layout','layoutSHA256'} and value['schema']==1 and
            value['binding']==binding(task) and value['binarySHA256']==digest(binary), 'census-abi-binding')
    layout = validate_layout(value['layout'])
    require(value['layoutSHA256']==hashlib.sha256(canonical(layout)).hexdigest(), 'census-abi-integrity')
    return layout


def decode(data, layout, uid, ruid, own_pid, own_birth, include_pid_one=False):
    size = layout['recordBytes']
    require(0 < len(data) <= CAP and len(data)%size==0, 'census-snapshot-length')
    seen, births, own, zombies, pid_one = set(), set(), None, 0, None
    for offset in range(0,len(data),size):
        row = {name:int.from_bytes(data[offset+field['offset']:offset+field['offset']+field['bytes']],
                                  'little',signed=field['signed']) for name,field in layout['fields'].items()}
        pid = row['pid']
        require(0 <= pid < 2**31 and pid not in seen, 'census-snapshot-pid')
        seen.add(pid)
        require(all(0 <= row[name] < 2**32-1 for name in ('uid','ruid')), 'census-snapshot-credentials')
        if include_pid_one and pid == 1:
            require(row['startSec'] > 0 and 0 <= row['startUsec'] < 1000000 and
                    row['state'] in (1,2,3,4), 'census-pid-one-identity')
            pid_one = (pid,row['startSec'],row['startUsec'],row['uid'],row['ruid'])
        if row['uid']!=uid and row['ruid']!=ruid:
            continue
        require(pid>0 and row['startSec']>0 and 0<=row['startUsec']<1000000 and
                row['state'] in (1,2,3,4,5), 'census-snapshot-identity')
        identity = (pid,row['startSec'],row['startUsec'],row['uid'],row['ruid'])
        if row['state']==5:
            zombies+=1
            continue
        births.add(identity)
        if pid==own_pid:
            own=identity
    require(own==(own_pid,*own_birth,uid,ruid), 'census-snapshot-self-binding')
    result = {'identities':births,'measurement':{'api':'sysctl-kern-proc-all','snapshotBytes':len(data),
            'snapshotEntries':len(seen),'candidateIdentities':len(births),'candidateZombies':zombies,
            'mixedCandidateIdentities':sum(row[3]!=uid or row[4]!=ruid for row in births),
            'selfBindingVerified':True,'rawProcessMetadataPublished':False}}
    if include_pid_one:
        require(pid_one is not None, 'census-pid-one-unavailable')
        result['pidOne'] = pid_one
    return result


def snapshot(layout, system=None, self_probe=None, include_pid_one=False):
    validate_layout(layout)
    system = system if system is not None else ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True)
    system.sysctl.argtypes = [ctypes.POINTER(ctypes.c_int),ctypes.c_uint,ctypes.c_void_p,
                             ctypes.POINTER(ctypes.c_size_t),ctypes.c_void_p,ctypes.c_size_t]
    system.sysctl.restype = ctypes.c_int
    if self_probe is None:
        from darwin_activity import probe
        self_probe = probe()
    state, own, reason = self_probe.bsd(os.getpid())
    require(state=='measured' and own.pid==os.getpid() and own.start_sec>0 and
            0<=own.start_usec<1000000 and own.uid==os.geteuid() and own.ruid==os.getuid(), 'census-self-unavailable')
    mib = (ctypes.c_int*4)(*layout['mib'])
    retries = []
    for attempt in range(3):
        needed = ctypes.c_size_t()
        ctypes.set_errno(0)
        code = system.sysctl(mib,4,None,ctypes.byref(needed),None,0)
        error = ctypes.get_errno()
        if code!=0 or error:
            if error==errno.ENOMEM and attempt<2:
                retries.append(error)
                continue
            if error in (errno.EPERM,errno.EACCES):
                raise Failure('census-sysctl-permission')
            raise Failure('census-sysctl-size-unavailable')
        require(0 < needed.value <= CAP and needed.value%layout['recordBytes']==0, 'census-sysctl-size')
        capacity = min(CAP,needed.value+32*layout['recordBytes'])
        data = ctypes.create_string_buffer(capacity)
        length = ctypes.c_size_t(capacity)
        ctypes.set_errno(0)
        code = system.sysctl(mib,4,data,ctypes.byref(length),None,0)
        error = ctypes.get_errno()
        if code!=0 or error:
            if error==errno.ENOMEM and attempt<2:
                retries.append(error)
                continue
            if error in (errno.EPERM,errno.EACCES):
                raise Failure('census-sysctl-permission')
            raise Failure('census-sysctl-data-unavailable')
        require(0 < length.value <= capacity, 'census-sysctl-returned-size')
        value = decode(data.raw[:length.value],layout,os.geteuid(),os.getuid(),os.getpid(),
                       (own.start_sec,own.start_usec),include_pid_one=include_pid_one)
        value['measurement'].update(attempts=attempt+1,retryErrnos=retries,
                                    layoutSHA256=hashlib.sha256(canonical(layout)).hexdigest())
        return value
    raise Failure('census-sysctl-attempts-exhausted')
