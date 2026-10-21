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
from qa_core import Failure, require, digest, write_json


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

    def anchor_deadline(self, maximum_seconds=None):
        deadline = self.monotonic_started + self.limits['jobMinutes'] * 60
        return deadline if maximum_seconds is None else min(
            deadline, time.monotonic() + maximum_seconds)

    @staticmethod
    def record_cleanup(diagnostics, operation, result, **extra):
        if diagnostics is None:
            return
        keys = ('status', 'state', 'reason', 'actualReaped', 'actualReturncode',
                'observedExitCode', 'signalAuthority', 'freshIdentityVerified', 'errorType', 'errno')
        diagnostics.append({'operation': operation,
                            **{key: result[key] for key in keys if key in result}, **extra})
        del diagnostics[:-16]

    def anchor_inventory(self, pgid, stage, *, deadline=None, diagnostics=None):
        from anchor_inventory import InventoryError, collect
        try:
            return collect(pgid, bootstrap_stage=stage if self.inventory_bootstrap else None,
                           deadline=deadline)
        except InventoryError as error:
            if diagnostics is not None:
                diagnostic = {**error.details, 'failureCode': str(error)}
                if 'failureCode' in error.details:
                    diagnostic['producerFailureCode'] = error.details['failureCode']
                diagnostics.append(diagnostic)
                del diagnostics[:-16]
            raise

    def observed_exit(self, anchor, diagnostics=None):
        value = anchor.observe(deadline=self.anchor_deadline())
        if value['status'] == 'live':
            return None
        if value['status'] == 'zombie' and type(value.get('observedExitCode')) is int:
            return value['observedExitCode']
        if value['status'] == 'reaped' and type(anchor.actual_returncode) is int:
            return anchor.actual_returncode
        self.record_cleanup(diagnostics, 'anchor-observation', value)
        raise Failure('owned-primary-observation-unavailable')

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
                        from darwin_inventory import progress_reply
                        value['queryFailure']['helperProgress']=progress_reply('\n'.join(result.stdout.splitlines()[:-1])+'\n')
                    else:
                        inventory=''.join(str(pgid)+' '+str(pid)+'\n' for pid in reply['pids'])
                else:
                    result = subprocess.run(['/bin/ps', '-axo', columns], capture_output=True,
                                            check=True, timeout=3, text=True)
            except subprocess.TimeoutExpired as error:
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
                if native:
                    from darwin_inventory import progress_reply
                    value['queryFailure']['helperProgress']=progress_reply(error.output)
            except Exception as error:
                if not native:
                    raise
                code=str(error) if isinstance(error,Failure) else 'inventory-helper-exception'
                allowed={'inventory-abi-missing','inventory-group-id','inventory-protocol-size',
                         'inventory-protocol','inventory-protocol-keys','inventory-protocol-queries',
                         'inventory-protocol-query','inventory-protocol-unknown','inventory-protocol-failure',
                         'inventory-protocol-pids','inventory-protocol-measurement','inventory-protocol-progress'}
                value={'rssMiB':None,'cpuSeconds':None,'rssLowerBoundMiB':None,'cpuLowerBoundSeconds':None,
                       'processes':None,'states':['?'],'_pids':(),'sampleComplete':False,
                       'unknownKinds':{'inventory-unavailable':1},'formatExamples':[],
                       'queryFailure':{'operation':'kern-proc-pgrp-inventory',
                           'failureCode':code if code in allowed else 'inventory-helper-exception'}}
                if isinstance(error,OSError) and type(error.errno) is int:
                    value['queryFailure']['errno']=error.errno
                if isinstance(error,subprocess.CalledProcessError):
                    value['queryFailure']['exitCode']=error.returncode
                    from darwin_inventory import progress_reply
                    value['queryFailure']['helperProgress']=progress_reply(error.output)
            else:
                if native and reply['status']=='measured':
                    from darwin_activity import probe
                    value=probe().group(inventory,pgid)
                    value['inventoryMeasurement']=reply['measurement']
                    from darwin_inventory import progress_reply
                    value['helperProgress']=progress_reply('\n'.join(result.stdout.splitlines()[:-1])+'\n')
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
        from primary_anchor import AnchorInterrupted, AnchorSpawnError, PrimaryAnchor
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
        anchor = None
        launch_complete = False
        progress = ProgressWatch(self.limits['stallQuietMinutes'])
        peak_rss, minimum_free, last_activity, quiet = None, None, None, 0
        phase = 'launch'
        failure_phase = None
        cleanup_failure = None
        cleanup_diagnostics = []
        leader_reap = {'status': 'not-launched'}
        group_cleanup = 'not-launched'
        post_reap_absence = {'status': 'unconfirmed', 'groupEmpty': None,
                            'readOnly': True, 'signalAuthority': False}
        cleanup_stopping_diagnostic = None
        cleanup_inventory_failure = None
        failure_evidence = None
        cleanup_attempted = False
        cleanup_stop_error = None
        def stop_primary_once():
            nonlocal cleanup_attempted, cleanup_stop_error
            if cleanup_attempted:
                if cleanup_stop_error is not None:
                    raise cleanup_stop_error
                return
            # One stage owns one INT/TERM/KILL cleanup sequence. In particular,
            # finally must not restart exhausted graces after a failed stop.
            cleanup_attempted = True
            try:
                self.stop(anchor, cleanup_diagnostics)
            except BaseException as error:
                cleanup_stop_error = error
                raise
        def collect_primary_group(pgid, *, deadline=None):
            nonlocal cleanup_inventory_failure
            from anchor_inventory import InventoryError
            try:
                return self.anchor_inventory(pgid, stage, deadline=deadline,
                                             diagnostics=cleanup_diagnostics)
            except InventoryError:
                # Keep the producer failure independently of the rolling tail
                # and later lifecycle/recovery receipts. Never infer emptiness.
                if cleanup_diagnostics and cleanup_diagnostics[-1].get('operation') == 'anchor-group-inventory':
                    cleanup_inventory_failure = dict(cleanup_diagnostics[-1])
                raise
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
                                    'leaderExitAlreadyObserved': anchor.observed_exit_code if anchor is not None else None}
                emit_excerpt('before-cleanup', before)
        try:
            with path.open('xb') as output:
                anchor = PrimaryAnchor.spawn(args, cwd=cwd, env=env, stdout=output,
                    stderr=subprocess.STDOUT,
                    bootstrap_stage=stage if self.inventory_bootstrap else None,
                    census=collect_primary_group)
                process = anchor.process
                launch_complete = True
                group_cleanup = 'unverified'
                self.groups.add(process.pid)
                next_status = 0
                while True:
                    phase = 'primary-observation'
                    if self.observed_exit(anchor, cleanup_diagnostics) is not None:
                        break
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
                        stop_primary_once()
                        break
                    phase = 'group-activity'
                    activity = (self.group_activity(process.pid,bootstrap=True) if self.inventory_bootstrap
                                else self.group_activity(process.pid))
                    self.observe_processes(activity)
                    last_activity = {key: value for key, value in activity.items() if not key.startswith('_')}
                    if activity.get('sampleAttempts', 1) > 1 or not activity['sampleComplete'] or activity.get('esrchRechecks'):
                        diagnostic = self.record_monitor(stage, activity)
                    if not activity['sampleComplete']:
                        phase = 'primary-observation'
                        if self.observed_exit(anchor, cleanup_diagnostics) is not None:
                            break
                        phase = 'group-activity'
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
                        stop_primary_once()
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
            code = self.observed_exit(anchor, cleanup_diagnostics)
            require(type(code) is int, 'owned-primary-exit-unavailable')
            if code:
                reason = reason or 'nonzero-exit'
                self.fail(stage, reason)
            retired = anchor.try_reap(deadline=self.anchor_deadline(5))
            self.record_cleanup(cleanup_diagnostics, 'anchor-retirement', retired)
            if anchor.lifecycle_state != 'reaped':
                reason = reason or ('owned-descendants-outlived-leader'
                    if retired.get('reason') == 'anchor-nonleaders-remain'
                    else 'owned-primary-reap-unconfirmed')
                self.fail(stage, reason)
                capture_failure_evidence()
                stop_primary_once()
            code = anchor.actual_returncode
            require(type(code) is int, 'owned-primary-exit-unavailable')
            if phase_deadline_minutes is not None and self.elapsed() >= phase_deadline_minutes * 60:
                reason = reason or 'phase-deadline'
            if code or reason:
                self.fail(stage, reason or 'nonzero-exit')
                raise Failure(reason or 'nonzero-exit')
            return path
        except BaseException as error:
            if anchor is None and isinstance(error, (AnchorInterrupted, AnchorSpawnError)) and error.anchor is not None:
                anchor = error.anchor
                process = anchor.process
                if anchor.receipt()['actualReaped'] is not True:
                    group_cleanup = 'unverified'
                    if type(anchor.pid) is int:
                        self.groups.add(anchor.pid)
            capture_failure_evidence()
            reason = reason or (str(error) if isinstance(error, (Failure, AnchorSpawnError,
                                AnchorInterrupted)) else type(error).__name__)
            failure_phase = phase
            if phase in ('group-activity', 'primary-observation') and self.monitor_failure is None:
                self.monitor_failure = self.record_monitor(stage, {
                    'errorType': type(error).__name__,
                    **({'component': 'primary-lifecycle'} if phase == 'primary-observation' else {})})
            self.fail(stage, reason)
            raise
        finally:
            try:
                if anchor is not None and (launch_complete or anchor.receipt()['actualReaped'] is not True):
                    try:
                        if anchor.lifecycle_state != 'reaped':
                            stop_primary_once()
                        absence_result = anchor.post_reap_absence(deadline=self.anchor_deadline(5))
                        self.record_cleanup(cleanup_diagnostics, 'anchor-post-reap-absence', absence_result)
                        post_reap_absence = {key: absence_result[key] for key in
                            ('status', 'groupEmpty', 'readOnly', 'signalAuthority')}
                        require(anchor.lifecycle_state == 'reaped' and
                                post_reap_absence['status'] == 'observed' and
                                post_reap_absence['groupEmpty'] is True,
                                'owned-primary-post-reap-group-unconfirmed')
                        group_cleanup = 'verified-gone'
                    finally:
                        if group_cleanup == 'verified-gone':
                            self.groups.discard(anchor.pid)
                        if self.native_registry is not None:
                            self.native_cleanup = self.native_registry.finish()
                            if self.native_cleanup['outlivedHarness'] or not self.native_cleanup['allOwnedGone']:
                                self.fail(stage, 'owned-native-cleanup-incomplete')
                                raise Failure('owned-native-cleanup-incomplete')
            except BaseException as error:
                capture_failure_evidence(before_cleanup=False)
                cleanup_failure = str(error) if isinstance(error, Failure) else type(error).__name__
                cleanup_stopping_diagnostic = (dict(cleanup_diagnostics[-1])
                                               if cleanup_diagnostics else None)
                reason = reason or cleanup_failure
                failure_phase = failure_phase or 'cleanup'
                self.fail(stage, reason)
                raise
            finally:
                # Failed authority never falls back to Popen's reaping methods.
                # The final recovery is a separate bounded exact-child attempt;
                # it supplies neither group retirement nor signal authority.
                if anchor is not None:
                    recovery_deadline = self.anchor_deadline(5)
                    while anchor.actual_returncode is None and time.monotonic() < recovery_deadline:
                        recovery = anchor.recover_exact_child(deadline=recovery_deadline)
                        self.record_cleanup(cleanup_diagnostics, 'anchor-exact-recovery', recovery)
                        if anchor.actual_returncode is not None or recovery.get('reason') not in (
                                'anchor-recovery-not-terminal', 'anchor-recovery-unavailable',
                                'anchor-recovery-not-ready', 'anchor-deadline'):
                            break
                        time.sleep(min(0.05, max(0, recovery_deadline - time.monotonic())))
                    code = anchor.actual_returncode
                    leader_reap = {'status': 'reaped' if type(code) is int else 'unavailable',
                                   'exitCode': code if type(code) is int else None}
                try:
                    log_bytes = path.stat().st_size
                except OSError:
                    log_bytes = None
                if failure_evidence is not None:
                    failure_evidence['afterCleanup'] = collect_excerpt()
                    emit_excerpt('after-cleanup', failure_evidence['afterCleanup'])
                actual_code = anchor.actual_returncode if anchor is not None else None
                self.records.append({'stage': stage, 'exitCode': actual_code if launch_complete and type(actual_code) is int else None,
                                     'launchCompleted': launch_complete,
                                     'failureEvidence': failure_evidence,
                                     'reason': reason, 'failurePhase': failure_phase,
                                     'cleanupFailure': cleanup_failure,
                                     'cleanupDiagnostics': cleanup_diagnostics,
                                     'cleanupStoppingDiagnostic': cleanup_stopping_diagnostic,
                                     'cleanupInventoryFailure': cleanup_inventory_failure,
                                     'groupCleanup': group_cleanup,
                                     'leaderReap': leader_reap,
                                     'primaryAnchor': ({
                                         'policy': 'retained-direct-child-v1',
                                         'retirementVerified': anchor.lifecycle_state == 'reaped',
                                         'actualReaped': anchor.receipt()['actualReaped'],
                                         'actualReturncode': anchor.actual_returncode,
                                         'signalAuthorityRevoked': not anchor.held,
                                         'postReapAbsence': post_reap_absence,
                                         'scope': 'active-process-group-epoch',
                                         'sessionOrDescendantClosure': 'unconfirmed'}
                                         if anchor is not None else None),
                                     'logBytes': log_bytes, 'peakRSSMiB': round(peak_rss) if peak_rss is not None else None,
                                     'minimumFreeGiB': round(minimum_free / 1024**3, 2) if minimum_free is not None else None,
                                     'lastActivity': last_activity, 'quietSeconds': round(quiet)})

    def stop(self, anchor, diagnostics=None):
        from primary_anchor import PrimaryAnchor
        require(isinstance(anchor, PrimaryAnchor), 'owned-primary-anchor-required')
        if anchor.lifecycle_state == 'reaped':
            return
        if not anchor.held:
            self.record_cleanup(diagnostics, 'anchor-stop-refused', anchor.receipt())
            raise Failure('owned-primary-anchor-unavailable')
        for value, grace in ((signal.SIGINT, self.limits['processCleanupGraceSeconds']),
                             (signal.SIGTERM, 10), (signal.SIGKILL, 5)):
            deadline = self.anchor_deadline(grace)
            require(time.monotonic() < deadline, 'owned-process-cleanup-timeout')
            retired = anchor.try_reap(deadline=deadline)
            self.record_cleanup(diagnostics, 'anchor-retirement', retired)
            if anchor.lifecycle_state == 'reaped':
                return
            require(anchor.held, 'owned-primary-anchor-unavailable')
            sent = anchor.signal_group(int(value), deadline=deadline)
            self.record_cleanup(diagnostics, 'anchor-group-signal', sent, signal=int(value))
            require(sent.get('reason') == 'anchor-signal-sent' and
                    sent.get('signalAuthority') is True, 'owned-primary-signal-unavailable')
            while time.monotonic() < deadline:
                retired = anchor.try_reap(deadline=deadline)
                if retired.get('reason') != 'anchor-not-terminal':
                    self.record_cleanup(diagnostics, 'anchor-retirement', retired)
                if anchor.lifecycle_state == 'reaped':
                    return
                require(anchor.held, 'owned-primary-anchor-unavailable')
                interval = 0.05 if retired.get('reason') == 'anchor-not-terminal' else 0.25
                time.sleep(min(interval, max(0, deadline - time.monotonic())))
        raise Failure('owned-process-cleanup-timeout')
