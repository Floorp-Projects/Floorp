# SPDX-License-Identifier: MPL-2.0
import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import signal
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from failure_excerpt import excerpt


class Failure(RuntimeError):
    pass


def require(condition, code):
    if not condition:
        raise Failure(code)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, sort_keys=True, indent=2) + '\n')


class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        require(urllib.parse.urlsplit(newurl).scheme == 'https', 'redirect-protocol')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def download(url, path, sha256, size=None, max_bytes=600 * 1024**2):
    require(urllib.parse.urlsplit(url).scheme == 'https', 'download-protocol')
    opener = urllib.request.build_opener(HTTPSRedirect)
    path = Path(path)
    require(not path.exists(), 'download-destination-exists')
    started = time.monotonic()
    with opener.open(url, timeout=30) as response, path.open('xb') as stream:
        count = 0
        while chunk := response.read(1024**2):
            count += len(chunk)
            require(count <= (size if size is not None else max_bytes), 'download-size-limit')
            require(time.monotonic() - started < 600, 'download-deadline')
            stream.write(chunk)
    require(size is None or count == size, 'download-size-mismatch')
    require(digest(path) == sha256, 'download-hash-mismatch')
    return {'bytes': count, 'sha256': sha256}


def scan_tar(path, max_bytes=4 * 1024**3, max_members=100000):
    rows, seen, logical = [], set(), 0
    with tarfile.open(path, 'r:*') as archive:
        for member in archive:
            name = PurePosixPath(member.name)
            require(not name.is_absolute() and '..' not in name.parts and '\\' not in member.name,
                    'archive-member-path')
            normalized = str(name)
            require(normalized not in seen, 'archive-duplicate-member')
            seen.add(normalized)
            require(len(seen) <= max_members, 'archive-member-limit')
            require(member.isfile() or member.isdir() or member.issym() or member.islnk(),
                    'archive-special-member')
            if member.isfile():
                logical += member.size
                require(logical <= max_bytes, 'archive-byte-limit')
                value = hashlib.file_digest(archive.extractfile(member), 'sha256').hexdigest()
                kind, size = 'file', member.size
            elif member.isdir():
                value, kind, size = '', 'dir', 0
            else:
                target = PurePosixPath(member.linkname)
                require(not target.is_absolute() and '\\' not in member.linkname,
                        'archive-absolute-link')
                parts = list(name.parent.parts) if member.issym() else []
                for part in target.parts:
                    if part == '..':
                        require(bool(parts), 'archive-escaping-link')
                        parts.pop()
                    elif part != '.':
                        parts.append(part)
                value = hashlib.sha256(member.linkname.encode()).hexdigest()
                kind, size = ('symlink' if member.issym() else 'hardlink'), member.size
            rows.append([normalized, kind, size, value])
    encoded = json.dumps(sorted(rows), ensure_ascii=False, separators=(',', ':')).encode()
    return {'members': len(rows), 'logicalBytes': logical,
            'canonicalSHA256': hashlib.sha256(encoded).hexdigest()}


def extract_tar(path, destination, max_bytes=4 * 1024**3):
    receipt = scan_tar(path, max_bytes=max_bytes)
    destination = Path(destination)
    destination.mkdir(mode=0o700, parents=True, exist_ok=False)
    with tarfile.open(path, 'r:*') as archive:
        archive.extractall(destination, filter='data')
    return receipt


def clean_environment(task, tools=None):
    names = ('HOME', 'USER', 'LOGNAME', 'TMPDIR', '__CF_USER_TEXT_ENCODING',
             'DISPLAY', 'SSH_AUTH_SOCK')
    result = {key: os.environ[key] for key in names if key in os.environ and key != 'SSH_AUTH_SOCK'}
    result.update({'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'LANG': 'en_US.UTF-8',
                   'LC_ALL': 'en_US.UTF-8', 'TMPDIR': str(task / 'tmp')})
    if tools:
        result['PATH'] = ':'.join([str(tools / x) for x in
            ('rust/bin', 'clang/bin', 'cbindgen', 'nasm', 'node/bin', 'dump_syms', 'fix-stacks')]
            + [result['PATH']])
        result.update({'MOZBUILD_STATE_PATH': str(task / 'mozbuild'),
                       'CARGO_HOME': str(task / 'cargo'), 'RUSTUP_HOME': str(task / 'rustup'),
                       'CC': str(tools / 'clang/bin/clang'), 'CXX': str(tools / 'clang/bin/clang++'),
                       'HOST_CC': str(tools / 'clang/bin/clang'),
                       'HOST_CXX': str(tools / 'clang/bin/clang++'),
                       'RUSTC': str(tools / 'rust/bin/rustc'), 'CARGO': str(tools / 'rust/bin/cargo'),
                       'LIBCLANG_PATH': str(tools / 'clang/lib'), 'MOZ_AUTOMATION': '1',
                       'CARGO_NET_OFFLINE': 'true', 'MOZ_PKG_FATAL_WARNINGS': '1'})
    return result


class ProgressWatch:
    def __init__(self, quiet_minutes):
        self.limit = quiet_minutes * 60
        self.previous = None
        self.last_progress = None

    def observe(self, now, log_bytes, free_bytes, activity):
        current = (log_bytes, free_bytes, activity['cpuSeconds'], activity['rssMiB'], activity['_pids'])
        previous = self.previous
        changed = (previous is None or current[0] != previous[0] or
                   abs(current[1] - previous[1]) >= 8 * 1024**2 or
                   current[2] != previous[2] or abs(current[3] - previous[3]) >= 1 or
                   current[4] != previous[4])
        if changed:
            self.last_progress = now
            self.previous = current
        quiet = now - self.last_progress
        stalled = (quiet >= self.limit and activity['processes'] > 0 and
                   'R' not in activity['states'])
        return quiet, stalled


class Runner:
    def __init__(self, task, started, limits):
        self.task, self.started, self.limits = Path(task), started, limits
        marker = self.task / 'started-monotonic'
        self.monotonic_started = float(marker.read_text()) if marker.is_file() else time.monotonic()
        self.groups = set()
        self.records = []
        self.failure = None
        self.native_registry = None
        self.native_cleanup = None
        self.monitor_diagnostics = []
        self.monitor_failure = None
        self.phase_deadline_minutes = None
        self.observed_processes = None
        self.observation_status = 'disabled'
        self.inventory_bootstrap = False

    def observe_processes(self, activity):
        """Bounded, sampled identity hints; never used to authorize signals."""
        if self.observed_processes is None:
            return
        try:
            self._observe_processes(activity)
        except Exception:
            self.observation_status = 'unavailable'

    def _observe_processes(self, activity):
        rows = activity.get('_pids', ())
        if (activity.get('counterSource') != 'libproc-taskallinfo' or
                not activity.get('sampleComplete') or
                not all(isinstance(row, tuple) and len(row) == 3 and
                        all(type(value) is int for value in row) and row[0] > 0 and
                        row[1] > 0 and 0 <= row[2] < 1000000 for row in rows)):
            self.observation_status = 'unavailable'
            return
        if len(self.observed_processes | set(rows)) > 100000:
            self.observation_status = 'truncated'
            return
        self.observed_processes.update(rows)
        if self.observation_status == 'disabled':
            self.observation_status = 'sampling'

    def elapsed(self):
        return max(0, time.monotonic() - self.monotonic_started)

    def fail(self, stage, code):
        if self.failure is None:
            path = self.task / 'first-failure.json'
            value = {'stage': stage, 'code': code, 'elapsedSeconds': round(self.elapsed(), 2)}
            self.failure = value
            candidate = self.task / ('.failure-' + uuid.uuid4().hex)
            try:
                with candidate.open('x') as stream:
                    stream.write(json.dumps(value) + '\n')
                    stream.flush()
                    os.fsync(stream.fileno())
                os.link(candidate, path)
            except FileExistsError:
                try:
                    self.failure = json.loads(path.read_text())
                except (ValueError, OSError):
                    self.failure['existingEvidenceInvalid'] = True
            except OSError as error:
                self.failure['persistenceError'] = error.errno
            finally:
                try:
                    candidate.unlink(missing_ok=True)
                except OSError:
                    pass

    @staticmethod
    def group_alive(pgid, diagnostics=None):
        try:
            os.killpg(pgid, 0)
            return True
        except ProcessLookupError:
            return False
        except OSError as error:
            if diagnostics is not None:
                diagnostics.append({'operation': 'group-probe', 'signal': 0,
                                    'errorType': type(error).__name__, 'errno': error.errno})
                del diagnostics[:-16]
            raise

    def owned_group_alive(self, process, diagnostics=None):
        was_unreaped = process.returncode is None
        process.poll()
        try:
            return self.group_alive(process.pid, diagnostics)
        except PermissionError as denied:
            # A newly exited, unreaped leader can leave a zombie-only Darwin
            # group. Only a new exact-child reap in this call permits a
            # fresh group probe, including a reap by the initial poll.
            observation = {'operation': 'denied-group-leader-reap',
                           'leaderWasUnreaped': was_unreaped,
                           'leaderExitAtProbe': process.returncode,
                           'reapResult': 'already-reaped' if not was_unreaped else 'pending',
                           'boundedWaitSeconds': None, 'freshProbeAttempted': False}
            if diagnostics is not None:
                diagnostics.append(observation)
                del diagnostics[:-16]
            if not was_unreaped:
                raise
            source = 'initial-poll'
            if process.returncode is None and process.poll() is None:
                # An exiting child may not yet be waitable at either poll.
                # Wait only for this exact child; timeout preserves denial.
                observation['boundedWaitSeconds'] = 0.05
                try:
                    process.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    observation['reapResult'] = 'wait-timeout'
                    raise denied from None
                source = 'bounded-wait'
            elif observation['leaderExitAtProbe'] is None:
                source = 'second-poll'
            if process.returncode is None:
                observation['reapResult'] = 'unconfirmed'
                raise denied
            observation.update(reapResult='newly-reaped', reapSource=source,
                               leaderExitAfterReap=process.returncode, freshProbeAttempted=True)
            return self.group_alive(process.pid, diagnostics)

    @staticmethod
    def parse_group_activity(text, pgid):
        require(len(text) <= 1024**2, 'process-inventory-limit')
        total, cpu, pids, states, rows, unknown, examples = 0, 0.0, [], set(), 0, {}, []
        for line in text.splitlines():
            fields = line.split()
            if not fields or fields[0] != str(pgid):
                continue
            rows += 1
            reason = None
            if len(fields) != 5:
                reason = 'column-count'
            elif not fields[1].isdecimal():
                reason = 'pid-unavailable'
            elif not fields[2].isdecimal():
                reason = 'rss-unavailable'
            elif not re.fullmatch(r'(?:[0-9]+-)?[0-9]+(?::[0-9]+){0,2}(?:\.[0-9]+)?', fields[3]):
                reason = 'cpu-unavailable'
            elif not re.fullmatch(r'[RSDTtWZXxKPIUH][A-Za-z+<>0-9-]{0,15}', fields[4]):
                reason = 'state-unavailable'
            if len(fields) > 1 and fields[1].isdecimal():
                pids.append(int(fields[1]))
            if reason:
                unknown[reason] = unknown.get(reason, 0) + 1
                states.add('?')
                if len(examples) < 3:
                    tokens = [value if re.fullmatch(r'[A-Za-z0-9?.:+<>-]{1,32}', value) else '<unavailable>'
                              for value in fields[2:6]]
                    examples.append({'kind': reason, 'normalizedIds': True,
                                     'row': '900 901 ' + ' '.join(tokens)})
                continue
            days, separator, clock = fields[3].partition('-')
            parts = (clock if separator else days).split(':')
            seconds = sum(float(value) * 60**i for i, value in enumerate(reversed(parts)))
            cpu += seconds + (int(days) * 86400 if separator else 0)
            total += int(fields[2])
            states.add(fields[4][0])
            if not examples:
                examples.append({'kind': 'complete', 'normalizedIds': True,
                                 'row': '900 901 ' + ' '.join(fields[2:])})
        if not rows:
            unknown['empty-group'] = 1
        return {'rssMiB': total / 1024 if not unknown else None,
                'cpuSeconds': round(cpu, 3) if not unknown else None,
                'rssLowerBoundMiB': total / 1024, 'cpuLowerBoundSeconds': round(cpu, 3),
                'processes': rows, 'states': sorted(states), '_pids': tuple(sorted(pids)),
                'sampleComplete': not unknown, 'unknownKinds': unknown, 'formatExamples': examples}

    @staticmethod
    def group_activity(pgid, *, bootstrap=False):
        partial = []
        for attempt in range(3):
            columns = ('pgid=,pid=,ppid=,uid=,ruid=,comm=' if sys.platform == 'darwin'
                       else 'pgid=,pid=,rss=,time=,stat=')
            query_started = time.monotonic()
            native = sys.platform == 'darwin' and not bootstrap
            try:
                if native:
                    from darwin_inventory import decode_reply
                    from process_census import BINDING_KEYS
                    require(type(pgid) is int and 0 < pgid < 2**31,'inventory-group-id')
                    env={key:os.environ[key] for key in
                         (*BINDING_KEYS,'TASK_ROOT','FLOORP_CENSUS_ABI','HOME','TMPDIR','LANG','LC_ALL')
                         if key in os.environ}
                    env['PYTHONDONTWRITEBYTECODE']='1'
                    if not all(key in env for key in (*BINDING_KEYS,'TASK_ROOT','FLOORP_CENSUS_ABI')):
                        raise Failure('inventory-abi-missing')
                    result=subprocess.run([sys.executable,str(Path(__file__).with_name('darwin_inventory.py')),
                                           '--pgid',str(pgid)],capture_output=True,check=True,timeout=3,
                                          text=True,env=env)
                    reply=decode_reply(result.stdout,pgid)
                    if reply['status']!='measured':
                        value={'rssMiB':None,'cpuSeconds':None,'rssLowerBoundMiB':None,
                               'cpuLowerBoundSeconds':None,'processes':None,'states':['?'],'_pids':(),
                               'sampleComplete':False,'unknownKinds':{'inventory-unavailable':1},
                               'formatExamples':[],'queryFailure':{'operation':'kern-proc-pgrp-inventory',
                                   'failureCode':reply['failureCode'],'phase':reply['phase'],
                                   'errorType':reply['errorType'],'queries':reply['queries']}}
                        if 'errno' in reply:value['queryFailure']['errno']=reply['errno']
                    else:
                        inventory=''.join(str(pgid)+' '+str(pid)+'\n' for pid in reply['pids'])
                else:
                    result = subprocess.run(['/bin/ps', '-axo', columns], capture_output=True,
                                            check=True, timeout=3, text=True)
            except subprocess.TimeoutExpired:
                # run() kills and reaps its exact query child before raising.
                # Partial output is not a complete process inventory. Retain
                # the failure inside the existing bounded sample retry budget.
                value = {'rssMiB': None, 'cpuSeconds': None,
                         'rssLowerBoundMiB': None, 'cpuLowerBoundSeconds': None,
                         'processes': None, 'states': ['?'], '_pids': (),
                         'sampleComplete': False, 'unknownKinds': {'inventory-timeout': 1},
                         'formatExamples': [],
                        'queryFailure': {'operation': 'kern-proc-pgrp-inventory' if native else 'process-group-inventory',
                             'errorType': 'TimeoutExpired', 'timeoutSeconds': 3,
                             'elapsedSeconds': round(time.monotonic() - query_started, 3)}}
            except Exception as error:
                if not native:
                    raise
                code=str(error) if isinstance(error,Failure) else 'inventory-helper-exception'
                allowed={'inventory-abi-missing','inventory-group-id','inventory-protocol-size',
                         'inventory-protocol','inventory-protocol-keys','inventory-protocol-queries',
                         'inventory-protocol-query','inventory-protocol-unknown','inventory-protocol-failure',
                         'inventory-protocol-pids','inventory-protocol-measurement'}
                value={'rssMiB':None,'cpuSeconds':None,'rssLowerBoundMiB':None,'cpuLowerBoundSeconds':None,
                       'processes':None,'states':['?'],'_pids':(),'sampleComplete':False,
                       'unknownKinds':{'inventory-unavailable':1},'formatExamples':[],
                       'queryFailure':{'operation':'kern-proc-pgrp-inventory',
                           'failureCode':code if code in allowed else 'inventory-helper-exception'}}
                if isinstance(error,OSError) and type(error.errno) is int:
                    value['queryFailure']['errno']=error.errno
                if isinstance(error,subprocess.CalledProcessError):
                    value['queryFailure']['exitCode']=error.returncode
            else:
                if native and reply['status']=='measured':
                    from darwin_activity import probe
                    value=probe().group(inventory,pgid)
                    value['inventoryMeasurement']=reply['measurement']
                elif not native and sys.platform == 'darwin':
                    from darwin_activity import probe
                    value = probe().group(result.stdout, pgid)
                elif not native:
                    value = Runner.parse_group_activity(result.stdout, pgid)
            value['inventoryBackend']='kern-proc-pgrp-helper' if native else 'bootstrap-ps' if bootstrap else 'linux-ps'
            if value['sampleComplete']:
                break
            partial.append({'unknownKinds': value['unknownKinds'], 'formatExamples': value['formatExamples'],
                            **{key: value[key] for key in ('esrchRechecks', 'esrchExamples', 'queryFailure')
                               if key in value},
                            **({'inventorySummary': value['inventorySummary']}
                               if 'inventorySummary' in value else {}),
                            'knownRSSLowerBoundMiB': value['rssLowerBoundMiB'],
                            'knownCPULowerBoundSeconds': value['cpuLowerBoundSeconds']})
            if attempt < 2:
                time.sleep(0.25)
        value.update({'sampleAttempts': attempt + 1, 'partialSamples': partial})
        return value

    @staticmethod
    def group_rss_mib(pgid):
        value = Runner.group_activity(pgid)
        require(value['sampleComplete'], 'monitor-unavailable')
        return value['rssMiB']

    def record_monitor(self, stage, activity):
        value = {'stage': stage, 'elapsedSeconds': round(self.elapsed(), 2),
                 **{key: item for key, item in activity.items() if not key.startswith('_')}}
        self.monitor_diagnostics.append(value)
        del self.monitor_diagnostics[:-16]
        return value

    def run(self, stage, args, cwd, env, deadline_minutes, log_limit=None, *, phase_deadline_minutes=None):
        require(stage.replace('-', '').isalnum(), 'stage-name')
        require(not self.inventory_bootstrap or stage in ('census-abi-compile','census-abi'),
                'census-abi-bootstrap-stage')
        require(deadline_minutes == self.limits['jobMinutes'], 'platform-budget-binding')
        if phase_deadline_minutes is None:
            phase_deadline_minutes = self.phase_deadline_minutes
        require(phase_deadline_minutes is None or
                0 < phase_deadline_minutes < deadline_minutes, 'phase-budget-binding')
        require(phase_deadline_minutes is None or self.elapsed() < phase_deadline_minutes * 60,
                'phase-deadline-before-launch')
        path = self.task / (stage + '.private.log')
        limit = log_limit or self.limits['maxStageLogBytes']
        reason = None
        process = None
        progress = ProgressWatch(self.limits['stallQuietMinutes'])
        peak_rss, minimum_free, last_activity, quiet = None, None, None, 0
        phase = 'launch'
        failure_phase = None
        cleanup_failure = None
        cleanup_diagnostics = []
        leader_reap = {'status': 'not-launched'}
        group_cleanup = 'not-launched'
        failure_evidence = None
        def collect_excerpt():
            try:
                return excerpt(path)
            except Exception:
                return {'available': False, 'errorType': 'diagnostic-collector-error',
                        'rawContentPublished': False}
        def emit_excerpt(phase_name, evidence):
            try:
                print(json.dumps({'failureExcerpt': {'stage': stage, 'phase': phase_name,
                                                     'evidence': evidence}}), flush=True)
            except Exception:
                pass
        def capture_failure_evidence(before_cleanup=True):
            nonlocal failure_evidence
            if failure_evidence is None:
                before = collect_excerpt() if before_cleanup else {
                    'available': False, 'reason': 'not-captured-before-cleanup'}
                failure_evidence = {'beforeCleanup': before,
                                    'leaderExitAlreadyObserved': process.returncode if process is not None else None}
                emit_excerpt('before-cleanup', before)
        try:
            with path.open('xb') as output:
                process = subprocess.Popen(args, cwd=cwd, env=env, stdout=output,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                group_cleanup = 'unverified'
                self.groups.add(process.pid)
                next_status = 0
                while process.poll() is None:
                    phase = 'resources'
                    elapsed = self.elapsed()
                    free = shutil.disk_usage(self.task).free
                    size = path.stat().st_size
                    private = self.task / 'native-private'
                    private_bytes = 0
                    for item in private.rglob('*') if private.is_dir() else ():
                        try:
                            if item.is_file() and not item.is_symlink():
                                private_bytes += item.stat().st_size
                        except FileNotFoundError:
                            pass
                    minimum_free = free if minimum_free is None else min(minimum_free, free)
                    if elapsed > deadline_minutes * 60:
                        reason = 'platform-job-limit'
                    elif free < self.limits['minFreeDuringBuildGiB'] * 1024**3:
                        reason = 'free-disk-reserve'
                    elif size > limit:
                        reason = 'private-log-limit'
                    elif private_bytes > self.limits['maxNativePrivateBytes']:
                        reason = 'native-private-size-limit'
                    elif phase_deadline_minutes is not None and elapsed >= phase_deadline_minutes * 60:
                        reason = 'phase-deadline'
                    if reason:
                        self.fail(stage, reason)
                        capture_failure_evidence()
                        self.stop(process, cleanup_diagnostics)
                        break
                    phase = 'group-activity'
                    activity = (self.group_activity(process.pid,bootstrap=True) if self.inventory_bootstrap
                                else self.group_activity(process.pid))
                    self.observe_processes(activity)
                    last_activity = {key: value for key, value in activity.items() if not key.startswith('_')}
                    if activity.get('sampleAttempts', 1) > 1 or not activity['sampleComplete'] or activity.get('esrchRechecks'):
                        diagnostic = self.record_monitor(stage, activity)
                    if not activity['sampleComplete']:
                        if process.poll() is not None:
                            break
                        if self.monitor_failure is None:
                            self.monitor_failure = diagnostic
                        lower_bound = activity.get('rssLowerBoundMiB')
                        if lower_bound is not None and lower_bound > self.limits.get('maxOwnedRSSMiB', 10240):
                            raise Failure('owned-process-memory-limit')
                        raise Failure('monitor-unavailable')
                    rss = activity['rssMiB']
                    phase = 'native-census'
                    if self.native_registry is not None:
                        self.native_registry.census()
                        rss += self.native_registry.outside_group_rss(activity['_pids'])
                    if activity.get('partialSamples') or activity.get('esrchRechecks'):
                        # An unobserved interval cannot count as evidence of
                        # unchanged CPU, memory, membership or runnable state.
                        progress = ProgressWatch(self.limits['stallQuietMinutes'])
                    phase = 'progress'
                    quiet, stalled = progress.observe(time.monotonic(), size, free, activity)
                    peak_rss = max(peak_rss or 0, rss)
                    if rss > self.limits.get('maxOwnedRSSMiB', 10240):
                        reason = 'owned-process-memory-limit'
                    elif stalled:
                        reason = 'stalled-no-progress'
                    if reason:
                        self.fail(stage, reason)
                        capture_failure_evidence()
                        self.stop(process, cleanup_diagnostics)
                        break
                    if elapsed >= next_status:
                        print(json.dumps({'stage': stage, 'elapsedMinutes': round(elapsed / 60, 1),
                                          'freeGiB': round(free / 1024**3, 1), 'logBytes': size,
                                          'ownedGroupRSSMiB': round(rss),
                                          'ownedGroupCPUSeconds': activity['cpuSeconds'],
                                          'processes': activity['processes'], 'states': activity['states'],
                                          'quietSeconds': round(quiet)}), flush=True)
                        next_status = elapsed + 30
                    time.sleep(1)
            phase = 'wait'
            code = process.wait(timeout=5)
            if phase_deadline_minutes is not None and self.elapsed() >= phase_deadline_minutes * 60:
                reason = reason or 'phase-deadline'
            if self.owned_group_alive(process, cleanup_diagnostics):
                reason = reason or 'owned-descendants-outlived-leader'
                self.fail(stage, reason)
                capture_failure_evidence()
                self.stop(process, cleanup_diagnostics)
            if code or reason:
                self.fail(stage, reason or 'nonzero-exit')
                raise Failure(reason or 'nonzero-exit')
            return path
        except BaseException as error:
            capture_failure_evidence()
            reason = reason or (str(error) if isinstance(error, Failure) else type(error).__name__)
            failure_phase = phase
            if phase == 'group-activity' and self.monitor_failure is None:
                self.monitor_failure = self.record_monitor(stage, {'errorType': type(error).__name__})
            self.fail(stage, reason)
            raise
        finally:
            try:
                if process is not None:
                    try:
                        if self.owned_group_alive(process, cleanup_diagnostics):
                            self.stop(process, cleanup_diagnostics)
                        group_cleanup = 'verified-gone'
                    finally:
                        self.groups.discard(process.pid)
                        if self.native_registry is not None:
                            self.native_cleanup = self.native_registry.finish()
                            if self.native_cleanup['outlivedHarness'] or not self.native_cleanup['allOwnedGone']:
                                self.fail(stage, 'owned-native-cleanup-incomplete')
                                raise Failure('owned-native-cleanup-incomplete')
            except BaseException as error:
                capture_failure_evidence(before_cleanup=False)
                cleanup_failure = str(error) if isinstance(error, Failure) else type(error).__name__
                reason = reason or cleanup_failure
                failure_phase = failure_phase or 'cleanup'
                self.fail(stage, reason)
                raise
            finally:
                # Reaping this exact Popen child needs no group access. A
                # denied group probe must remain an error, but must not skip
                # the independent bounded wait. No extra signal is sent.
                if process is not None:
                    try:
                        code = process.wait(timeout=5)
                        leader_reap = {'status': 'reaped', 'exitCode': code}
                    except subprocess.TimeoutExpired:
                        leader_reap = {'status': 'still-running', 'exitCode': None}
                    except OSError as error:
                        leader_reap = {'status': 'unavailable', 'exitCode': None,
                                       'errorType': type(error).__name__, 'errno': error.errno}
                try:
                    log_bytes = path.stat().st_size
                except OSError:
                    log_bytes = None
                if failure_evidence is not None:
                    failure_evidence['afterCleanup'] = collect_excerpt()
                    emit_excerpt('after-cleanup', failure_evidence['afterCleanup'])
                self.records.append({'stage': stage, 'exitCode': process.returncode if process is not None else None,
                                     'failureEvidence': failure_evidence,
                                     'reason': reason, 'failurePhase': failure_phase,
                                     'cleanupFailure': cleanup_failure,
                                     'cleanupDiagnostics': cleanup_diagnostics,
                                     'groupCleanup': group_cleanup,
                                     'leaderReap': leader_reap,
                                     'logBytes': log_bytes, 'peakRSSMiB': round(peak_rss) if peak_rss is not None else None,
                                     'minimumFreeGiB': round(minimum_free / 1024**3, 2) if minimum_free is not None else None,
                                     'lastActivity': last_activity, 'quietSeconds': round(quiet)})

    def stop(self, process, diagnostics=None):
        for value, grace in ((signal.SIGINT, self.limits['processCleanupGraceSeconds']),
                             (signal.SIGTERM, 10), (signal.SIGKILL, 5)):
            if not self.owned_group_alive(process, diagnostics):
                process.poll()
                return
            try:
                os.killpg(process.pid, value)
            except ProcessLookupError:
                break
            except OSError as error:
                if diagnostics is not None:
                    diagnostics.append({'operation': 'group-signal', 'signal': int(value),
                                        'errorType': type(error).__name__, 'errno': error.errno})
                    del diagnostics[:-16]
                raise
            deadline = time.monotonic() + grace
            while self.owned_group_alive(process, diagnostics) and time.monotonic() < deadline:
                time.sleep(0.05)
        require(not self.owned_group_alive(process, diagnostics), 'owned-process-cleanup-timeout')
