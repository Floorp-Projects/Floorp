# SPDX-License-Identifier: MPL-2.0
"""Read task-level counters without ps's per-thread state collection."""

import ctypes
import errno
import hashlib
import os


class BSDInfo(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in
                ('flags', 'status', 'xstatus', 'pid', 'ppid', 'uid', 'gid', 'ruid',
                 'rgid', 'svuid', 'svgid', 'reserved')]
    _fields_ += [('comm', ctypes.c_char * 16), ('name', ctypes.c_char * 32)]
    _fields_ += [(name, ctypes.c_uint32) for name in ('nfiles', 'pgid', 'jobc', 'tdev', 'tpgid')]
    _fields_ += [('nice', ctypes.c_int32), ('start_sec', ctypes.c_uint64),
                 ('start_usec', ctypes.c_uint64)]


class TaskInfo(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint64) for name in
                ('virtual_size', 'resident_size', 'total_user', 'total_system',
                 'threads_user', 'threads_system')]
    _fields_ += [(name, ctypes.c_int32) for name in
                 ('policy', 'faults', 'pageins', 'cow_faults', 'messages_sent',
                  'messages_received', 'syscalls_mach', 'syscalls_unix', 'csw',
                  'threadnum', 'numrunning', 'priority')]


class TaskAllInfo(ctypes.Structure):
    _fields_ = [('bsd', BSDInfo), ('task', TaskInfo)]


class Timebase(ctypes.Structure):
    _fields_ = [('numer', ctypes.c_uint32), ('denom', ctypes.c_uint32)]


class DarwinActivity:
    def __init__(self, lib=None, system=None):
        if (ctypes.sizeof(BSDInfo), ctypes.sizeof(TaskInfo), ctypes.sizeof(TaskAllInfo),
                TaskAllInfo.task.offset, BSDInfo.start_sec.offset) != (136, 96, 232, 136, 120):
            raise RuntimeError('darwin-task-info-abi')
        self.lib = lib if lib is not None else ctypes.CDLL('/usr/lib/libproc.dylib', use_errno=True)
        self.lib.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64,
                                         ctypes.c_void_p, ctypes.c_int]
        self.lib.proc_pidinfo.restype = ctypes.c_int
        system = system if system is not None else ctypes.CDLL('/usr/lib/libSystem.B.dylib')
        system.mach_timebase_info.argtypes = [ctypes.POINTER(Timebase)]
        system.mach_timebase_info.restype = ctypes.c_int
        base = Timebase()
        if system.mach_timebase_info(ctypes.byref(base)) != 0 or not base.numer or not base.denom:
            raise RuntimeError('darwin-timebase-unavailable')
        self.numer, self.denom = base.numer, base.denom
        self.tag_key = os.urandom(16)

    def tag(self, pid):
        return hashlib.blake2s(str(pid).encode(), key=self.tag_key, digest_size=8).hexdigest()

    def query(self, pid, flavor, value):
        if not isinstance(pid, int) or pid <= 0:
            raise ValueError('positive-process-id-required')
        ctypes.set_errno(0)
        size = self.lib.proc_pidinfo(pid, flavor, 1 if flavor == 3 else 0,
                                     ctypes.byref(value), ctypes.sizeof(value))
        return size, ctypes.get_errno()

    def bsd(self, pid, details=None):
        value = BSDInfo()
        size, code = self.query(pid, 3, value)
        if details is not None:
            details.update(bytes=size, expectedBytes=ctypes.sizeof(value), errno=code)
        if size == ctypes.sizeof(value):
            if value.pid != pid or not value.start_sec or value.start_usec >= 1000000:
                return 'unavailable', None, 'invalid-identity'
            if value.status == 5:
                return 'gone', value, 'bsd-zombie'
            if value.status in (1, 2, 3, 4):
                return 'measured', value, None
            return 'unavailable', None, 'invalid-bsd-state'
        if code in (errno.EPERM, errno.EACCES):
            return 'unavailable', None, 'permission'
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return 'gone', None, 'signal-zero-esrch'
        except PermissionError:
            return 'unavailable', None, 'permission'
        return 'unavailable', None, 'short-bsd-info'

    def read(self, pid, pgid=None, expected=None):
        result = {'status': 'unavailable', 'pidTag': self.tag(pid)}
        value = TaskAllInfo()
        size, code = self.query(pid, 2, value)
        result['taskQuery'] = {'bytes': size, 'expectedBytes': ctypes.sizeof(value), 'errno': code}
        if size != ctypes.sizeof(value):
            result['reason'] = 'permission' if code in (errno.EPERM, errno.EACCES) else 'short-task-info'
            if code in (errno.EPERM, errno.EACCES):
                return result
            details = {}
            state, bsd, reason = self.bsd(pid, details)
            result.update(bsdQuery=details, fallbackReason=reason or state)
            if bsd is not None:
                result['identityTag'] = self.tag((bsd.pid, bsd.start_sec, bsd.start_usec))
            if bsd is not None and expected is not None and (
                    bsd.pid, bsd.start_sec, bsd.start_usec) != (
                    expected['pid'], expected['startSec'], expected['startUsec']):
                return {**result, 'status': 'gone', 'reason': 'generation-replaced'}
            if state == 'measured' and (bsd.uid != os.geteuid() or (pgid is not None and bsd.pgid != pgid)):
                return {**result, 'reason': 'membership-changed'}
            if state == 'gone':
                return {**result, 'status': 'gone', 'reason': reason}
            if reason == 'permission':
                return {**result, 'reason': reason}
            return result
        bsd, task = value.bsd, value.task
        birth = (bsd.pid, bsd.start_sec, bsd.start_usec)
        if bsd.pid != pid or not bsd.start_sec or bsd.start_usec >= 1000000:
            return {**result, 'reason': 'invalid-identity'}
        result['identityTag'] = self.tag(birth)
        if bsd.status not in (1, 2, 3, 4, 5):
            return {**result, 'reason': 'invalid-bsd-state'}
        if expected is not None and birth != (expected['pid'], expected['startSec'], expected['startUsec']):
            return {**result, 'status': 'gone', 'reason': 'generation-replaced'}
        if bsd.status == 5:
            return {**result, 'status': 'gone', 'reason': 'bsd-zombie'}
        if bsd.uid != os.geteuid() or (pgid is not None and bsd.pgid != pgid):
            return {**result, 'reason': 'membership-changed'}
        if (bsd.status not in (1, 2, 3, 4) or task.policy != 1 or not task.virtual_size or
                task.threadnum <= 0 or not 0 <= task.numrunning <= task.threadnum):
            return {**result, 'reason': 'invalid-task-counters'}
        seconds = (task.total_user + task.total_system) * self.numer / self.denom / 1e9
        return {**result, 'status': 'measured', 'rssMiB': task.resident_size / 1024**2,
                'cpuSeconds': seconds, 'state': 'T' if bsd.status == 4 else 'R' if task.numrunning else 'S',
                '_birth': birth}

    def group(self, inventory, pgid):
        if len(inventory) > 1024**2:
            raise RuntimeError('process-inventory-limit')
        pids, unknown, examples = [], {}, []
        rss, cpu, states, births, gone = 0.0, 0.0, set(), [], 0
        for line in inventory.splitlines():
            fields = line.split()
            if not fields or fields[0] != str(pgid):
                continue
            if len(fields) != 2 or not fields[1].isdecimal() or int(fields[1]) <= 0:
                unknown['inventory-format'] = unknown.get('inventory-format', 0) + 1
                continue
            pid = int(fields[1])
            if pid in pids:
                unknown['duplicate-pid'] = unknown.get('duplicate-pid', 0) + 1
                continue
            pids.append(pid)
            value = self.read(pid, pgid)
            if value['status'] == 'unavailable':
                reason = value['reason']
                unknown[reason] = unknown.get(reason, 0) + 1
                if len(examples) < 3:
                    examples.append({'kind': reason, 'pidTag': value['pidTag'],
                                     'identityTag': value.get('identityTag'), 'source': 'libproc-taskallinfo',
                                     **{key: value[key] for key in ('taskQuery', 'bsdQuery', 'fallbackReason')
                                        if key in value}})
            elif value['status'] == 'gone':
                gone += 1
            else:
                rss += value['rssMiB']
                cpu += value['cpuSeconds']
                states.add(value['state'])
                births.append(value['_birth'])
                if not examples:
                    examples.append({'kind': 'measured', 'source': 'libproc-taskallinfo',
                                     'rssMiB': value['rssMiB'], 'cpuSeconds': value['cpuSeconds'],
                                     'state': value['state'], 'pidTag': value['pidTag'],
                                     'identityTag': value['identityTag']})
        if not pids:
            unknown['empty-group'] = 1
        complete = not unknown
        return {'rssMiB': rss if complete else None, 'cpuSeconds': round(cpu, 3) if complete else None,
                'rssLowerBoundMiB': rss, 'cpuLowerBoundSeconds': round(cpu, 3),
                'processes': len(births), 'states': sorted(states | ({'?'} if unknown else set())),
                '_pids': tuple(sorted(births)), 'sampleComplete': complete, 'unknownKinds': unknown,
                'formatExamples': examples, 'confirmedGone': gone, 'counterSource': 'libproc-taskallinfo',
                'statesSource': 'libproc-runnable-summary',
                'timebaseNumer': self.numer, 'timebaseDenom': self.denom}


_probe = None


def probe():
    global _probe
    if _probe is None:
        _probe = DarwinActivity()
    return _probe
