# SPDX-License-Identifier: MPL-2.0
import ctypes
import errno
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import signal
import subprocess
import time
from support import Failure, require, write_json
from darwin_activity import probe

PASSIVE_SETTLE_SECONDS = 5


class MacProcesses:
    def __init__(self, harness):
        self.activity = probe()
        self.lib = self.activity.lib
        self.lib.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        self.lib.proc_pidpath.restype = ctypes.c_int
        self.lib.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64,
                                         ctypes.c_void_p, ctypes.c_int]
        self.lib.proc_pidinfo.restype = ctypes.c_int

    def path(self, pid):
        buffer = ctypes.create_string_buffer(4096)
        ctypes.set_errno(0)
        size = self.lib.proc_pidpath(pid, buffer, len(buffer))
        if size > 0:
            return os.fsdecode(buffer.value)
        code = ctypes.get_errno()
        require(code in (0, errno.ESRCH, errno.ENOENT), 'native-path-unavailable')
        require(self.info(pid) is None, 'native-live-path-unavailable')
        return None

    def info(self, pid):
        status, value, reason = self.activity.bsd(pid)
        require(status != 'unavailable', 'native-birth-unavailable')
        return value if status == 'measured' else None

    def pids(self):
        value = subprocess.run(['/bin/ps', '-axo', 'pid=,uid='], capture_output=True, text=True,
                               check=True, timeout=3).stdout
        require(len(value) < 65536, 'native-process-inventory-limit')
        result = [int(parts[0]) for line in value.splitlines() if len(parts := line.split()) == 2
                  and all(x.isdecimal() for x in parts) and int(parts[1]) == os.geteuid()]
        require(len(result) <= 4096, 'native-process-count-limit')
        return result

    def describe(self, pid):
        before = self.info(pid)
        if before is None:
            return None
        if before.uid != os.geteuid():
            return {'pid': pid, 'uid': before.uid, 'startSec': before.start_sec,
                    'startUsec': before.start_usec, 'exe': '<unavailable>', 'argvSHA256': '<unavailable>'}
        path = self.path(pid)
        if before is None or path is None:
            return None
        process = subprocess.run(['/bin/ps', '-ww', '-p', str(pid), '-o', 'command='],
                                  capture_output=True, text=True, timeout=3)
        after = self.info(pid)
        if after is None:
            return None
        require(process.returncode == 0 and len(process.stdout) < 65536, 'native-argv-unavailable')
        require((before.pid, before.start_sec, before.start_usec) ==
                (after.pid, after.start_sec, after.start_usec), 'native-pid-generation-changed')
        command = process.stdout.strip()
        return {'pid': pid, 'uid': after.uid, 'startSec': after.start_sec, 'startUsec': after.start_usec,
                'pgid': after.pgid, 'ppid': after.ppid, 'exe': path, 'argv': command,
                'argvSHA256': hashlib.sha256(command.encode()).hexdigest()}

    def rss(self, pid, expected):
        value = self.activity.read(pid, expected=expected)
        require(value['status'] != 'unavailable', 'native-rss-unavailable')
        return value['rssMiB'] if value['status'] == 'measured' else 0


def identity(value):
    return tuple(value[key] for key in ('pid', 'uid', 'startSec', 'startUsec', 'exe', 'argvSHA256'))


class OwnedNative:
    def __init__(self, task, browser, harness, started, helpers=()):
        self.task, self.browser, self.started = task, browser, started
        executable = plistlib.loads((browser / 'Contents/Info.plist').read_bytes())['CFBundleExecutable']
        self.exe = str((browser / 'Contents/MacOS' / executable).resolve())
        self.private = (task / 'native-private').resolve()
        self.helper_exes = {str((browser / relative).resolve()) for relative in helpers}
        self.system = MacProcesses(harness)
        self.records = {}
        self.finished = False
        self.proof = None

    def register(self, value, kind):
        key = (value['pid'], value['startSec'], value['startUsec'])
        if key not in self.records:
            self.records[key] = {k: v for k, v in value.items() if k != 'argv'}
            self.records[key]['kind'] = kind
            write_json(self.task / 'owned-native.private.json', list(self.records.values()))

    def census(self):
        candidates = []
        for pid in self.system.pids():
            path = self.system.path(pid)
            if (path and Path(path).is_relative_to(self.browser.resolve())) or (path and Path(path).is_relative_to(self.private) and
                                    path.endswith('/Contents/MacOS/floorp-app-shim')):
                value = self.system.describe(pid)
                if value is not None:
                    require(value['uid'] == os.geteuid() and
                            value['startSec'] + value['startUsec'] / 1000000 >= self.started,
                            'native-unowned-generation')
                    candidates.append(value)
        for value in candidates:
            if value['exe'] != self.exe:
                continue
            require(value['argv'].startswith(self.exe + ' '), 'native-host-argv-prefix')
            args = value['argv'][len(self.exe) + 1:].split()
            require(len(args) == 6 and args[0:2] == ['--no-remote', '--profile'] and
                    args[3:] == ['--marionette', '--remote-allow-system-access', 'about:blank'],
                    'native-host-argv')
            profile = Path(args[2]).resolve()
            require(profile.is_relative_to(self.private) and profile.name == 'profile' and
                    profile.is_dir() and profile.stat().st_uid == os.geteuid(), 'native-host-profile')
            value['profile'] = str(profile)
            self.register(value, 'host')
        host_pids = {row['pid'] for row in self.records.values() if row['kind'] == 'host'}
        pending = [value for value in candidates if value['exe'] != self.exe and
                   Path(value['exe']).is_relative_to(self.browser.resolve())]
        require(all(row['exe'] in self.helper_exes for row in pending), 'native-unrecognized-app-helper')
        while pending:
            remaining = []
            for value in pending:
                if any(identity(row) == identity(value) for row in self.records.values()):
                    continue
                parents = [row for row in self.records.values() if row['pid'] == value['ppid'] and
                           row['kind'] in ('host', 'helper') and
                           (row['startSec'], row['startUsec']) <= (value['startSec'], value['startUsec'])]
                parent = max(parents, key=lambda row: (row['startSec'], row['startUsec'])) if parents else None
                current = self.system.describe(value['ppid']) if parent is not None else None
                if current is not None and identity(parent) == identity(current):
                    self.register(value, 'helper')
                else:
                    remaining.append(value)
            require(len(remaining) < len(pending), 'native-helper-parent-generation-unavailable')
            pending = remaining
        for value in candidates:
            if Path(value['exe']).is_relative_to(self.browser.resolve()):
                continue
            require(value['argv'].startswith(value['exe'] + ' '), 'native-shim-argv-prefix')
            args = value['argv'][len(value['exe']) + 1:].split()
            require(len(args) == 6 and set(args[::2]) == {'--host-service', '--host-pid', '--launch-token'},
                    'native-shim-argv')
            flags = dict(zip(args[::2], args[1::2]))
            require(flags['--host-pid'].isdecimal() and int(flags['--host-pid']) in host_pids and
                    re.fullmatch('[0-9a-f]{64}', flags['--launch-token']) and
                    re.fullmatch(r'\S{1,256}', flags['--host-service']), 'native-shim-host-binding')
            metadata = plistlib.loads((Path(value['exe']).parents[1] / 'Info.plist').read_bytes())
            host = max((row for row in self.records.values() if row['kind'] == 'host' and
                        row['pid'] == int(flags['--host-pid'])), key=lambda row: (row['startSec'], row['startUsec']))
            require(Path(metadata['FloorpAppShimHostPath']).resolve() == self.browser.resolve() and
                    str(Path(metadata['FloorpAppShimProfilePath']).resolve()) == host['profile'],
                    'native-shim-profile-binding')
            self.register(value, 'shim')

    def live(self, errors=None):
        result = []
        for row in self.records.values():
            try:
                current = self.system.describe(row['pid'])
                if current is not None:
                    same_birth = all(current[key] == row[key] for key in ('pid', 'startSec', 'startUsec'))
                    if same_birth:
                        require(identity(current) == identity(row), 'owned-generation-identity-changed')
                        result.append(row)
            except Exception as error:
                if errors is None:
                    raise
                errors.add(str(error) if isinstance(error, Failure) else type(error).__name__)
        return result

    def outside_group_rss(self, measured_births):
        measured = set(measured_births)
        # Group membership can change after sampling. Only the exact births
        # already counted in that sample may be omitted here.
        return sum(self.system.rss(row['pid'], row) for row in self.live()
                   if (row['pid'], row['startSec'], row['startUsec']) not in measured)

    def finish(self):
        if self.finished:
            return self.proof
        errors = set()
        def census():
            try:
                self.census()
            except Exception as error:
                errors.add(str(error) if isinstance(error, Failure) else type(error).__name__)
        census()
        left = self.live(errors)
        initial_left = len(left)
        settle = time.monotonic() + PASSIVE_SETTLE_SECONDS
        while left and time.monotonic() < settle:
            time.sleep(0.1)
            left = self.live(errors)
        original_left = len(left)
        signaled = 0
        for sig, seconds in ((signal.SIGTERM, 10), (signal.SIGKILL, 5)):
            for row in left:
                try:
                    current = self.system.describe(row['pid'])
                    if current is not None and identity(current) == identity(row):
                        os.kill(row['pid'], sig)
                        signaled += 1
                except ProcessLookupError:
                    pass
                except Exception as error:
                    errors.add(str(error) if isinstance(error, Failure) else type(error).__name__)
            deadline = time.monotonic() + seconds
            while left and time.monotonic() < deadline:
                time.sleep(0.1)
                left = self.live(errors)
            if not left:
                break
        census()
        left = self.live(errors)
        self.proof = {'hostsObserved': sum(r['kind'] == 'host' for r in self.records.values()),
                      'shimsObserved': sum(r['kind'] == 'shim' for r in self.records.values()),
                      'helpersObserved': sum(r['kind'] == 'helper' for r in self.records.values()),
                      'pendingAtHarnessReturn': initial_left,
                      'outlivedHarness': original_left, 'signalsToVerifiedOwnedInstances': signaled,
                      'allOwnedGone': not left and not errors, 'censusComplete': not errors,
                      'errors': sorted(errors)[:16],
                      'privateIdentitiesPublished': False}
        self.finished = True
        return self.proof
