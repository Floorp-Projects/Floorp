# SPDX-License-Identifier: MPL-2.0
"""Read current-user native test identities without starting a setuid helper."""

import ctypes
import errno
import os
from pathlib import Path
import struct
import sys
import time


class BSDInfo(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint32) for name in
                ('flags', 'status', 'xstatus', 'pid', 'ppid', 'uid', 'gid', 'ruid',
                 'rgid', 'svuid', 'svgid', 'reserved')]
    _fields_ += [('comm', ctypes.c_char * 16), ('name', ctypes.c_char * 32)]
    _fields_ += [(name, ctypes.c_uint32) for name in ('nfiles', 'pgid', 'jobc', 'tdev', 'tpgid')]
    _fields_ += [('nice', ctypes.c_int32), ('start_sec', ctypes.c_uint64), ('start_usec', ctypes.c_uint64)]


def require(value, code):
    if not value:
        raise RuntimeError(code)


def decode_argv(data):
    require(4 < len(data) <= 65536, 'native-argv-size')
    argc = struct.unpack_from('=i', data)[0]
    require(0 < argc <= 4096, 'native-argv-count')
    cursor = data.find(b'\0', 4)
    require(cursor > 4, 'native-argv-executable')
    cursor += 1
    while cursor < len(data) and data[cursor] == 0:
        cursor += 1
    args = []
    for _ in range(argc):
        end = data.find(b'\0', cursor)
        require(end >= cursor, 'native-argv-truncated')
        args.append(os.fsdecode(data[cursor:end]))
        cursor = end + 1
    require(bool(args[0]), 'native-argv-zero')
    return args


class NativeProcesses:
    def __init__(self, lib=None, system=None):
        require(ctypes.sizeof(BSDInfo) == 136 and BSDInfo.start_sec.offset == 120,
                'native-bsd-abi')
        require(lib is not None or sys.platform == 'darwin', 'native-process-platform')
        self.lib = lib if lib is not None else ctypes.CDLL('/usr/lib/libproc.dylib', use_errno=True)
        self.system = system if system is not None else ctypes.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
        self.lib.proc_listpids.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int]
        self.lib.proc_listpids.restype = ctypes.c_int
        self.lib.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
        self.lib.proc_pidinfo.restype = ctypes.c_int
        self.lib.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        self.lib.proc_pidpath.restype = ctypes.c_int
        self.system.sysctl.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_uint,
                                      ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t),
                                      ctypes.c_void_p, ctypes.c_size_t]
        self.system.sysctl.restype = ctypes.c_int

    def pids(self):
        values = (ctypes.c_int * 4096)()
        ctypes.set_errno(0)
        count = self.lib.proc_listpids(4, os.geteuid(), values, ctypes.sizeof(values))
        require(ctypes.get_errno() == 0 and 0 < count < ctypes.sizeof(values) and count % 4 == 0,
                'native-process-list-unavailable')
        result = [value for value in values[:count // 4] if value > 0]
        require(bool(result) and len(result) == len(set(result)), 'native-process-list-invalid')
        return result

    def bsd(self, pid):
        require(type(pid) is int and pid > 0, 'native-positive-pid')
        value = BSDInfo()
        ctypes.set_errno(0)
        size = self.lib.proc_pidinfo(pid, 3, 1, ctypes.byref(value), ctypes.sizeof(value))
        if size != ctypes.sizeof(value):
            code = ctypes.get_errno()
            if code not in (errno.EPERM, errno.EACCES):
                try:
                    os.kill(pid, 0)
                except ProcessLookupError:
                    return None
            raise RuntimeError('native-birth-unavailable')
        require(value.pid == pid and value.start_sec > 0 and value.start_usec < 1000000 and
                value.status in (1, 2, 3, 4, 5), 'native-birth-invalid')
        require(value.uid == os.geteuid(), 'native-owner-changed')
        return None if value.status == 5 else value

    def path(self, pid):
        value = ctypes.create_string_buffer(4096)
        ctypes.set_errno(0)
        size = self.lib.proc_pidpath(pid, value, len(value))
        require(0 < size < len(value) and bool(value.value), 'native-path-unavailable')
        return os.fsdecode(value.value)

    def argv(self, pid):
        data = ctypes.create_string_buffer(65536)
        size = ctypes.c_size_t(len(data))
        mib = (ctypes.c_int * 3)(1, 49, pid)
        ctypes.set_errno(0)
        code = self.system.sysctl(mib, 3, data, ctypes.byref(size), None, 0)
        require(code == 0 and ctypes.get_errno() == 0 and 4 < size.value < len(data),
                'native-argv-unavailable')
        return decode_argv(data.raw[:size.value])

    def rows(self, pid=None, executable=None):
        deadline = time.monotonic() + 5
        rows = []
        for current in self.pids() if pid is None else [pid]:
            require(time.monotonic() < deadline, 'native-process-list-deadline')
            before = self.bsd(current)
            if before is None:
                continue
            try:
                path = self.path(current)
                if executable is not None and path != executable:
                    continue
                first = self.argv(current)
                second = self.argv(current)
                after = self.bsd(current)
                if after is None:
                    continue
                require((before.pid, before.uid, before.start_sec, before.start_usec) ==
                        (after.pid, after.uid, after.start_sec, after.start_usec) and
                        path == self.path(current) and first == second, 'native-process-identity-changed')
                require(Path(first[0]).resolve() == Path(path).resolve(), 'native-process-argv-path')
            except RuntimeError:
                if self.bsd(current) is None:
                    continue
                raise
            rows.append((current, f'{after.start_sec}.{after.start_usec:06}', tuple(first)))
        return rows
