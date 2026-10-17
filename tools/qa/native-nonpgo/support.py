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
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid


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
    def group_alive(pgid):
        try:
            os.killpg(pgid, 0)
            return True
        except ProcessLookupError:
            return False

    @staticmethod
    def group_activity(pgid):
        result = subprocess.run(['/bin/ps', '-axo', 'pgid=,pid=,rss=,time=,stat='], capture_output=True,
                                check=True, timeout=3, text=True)
        require(len(result.stdout) <= 1024**2, 'process-inventory-limit')
        total, cpu, pids, states = 0, 0.0, [], set()
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) != 5 or fields[0] != str(pgid):
                continue
            require(fields[1].isdecimal() and fields[2].isdecimal() and
                    re.fullmatch(r'[0-9:-]+(?:\.[0-9]+)?', fields[3]) and
                    re.fullmatch(r'[A-Za-z+<>0-9-]{1,16}', fields[4]),
                    'process-activity-format')
            days, separator, clock = fields[3].partition('-')
            parts = (clock if separator else days).split(':')
            require(1 <= len(parts) <= 3, 'process-cpu-time')
            seconds = sum(float(value) * 60**i for i, value in enumerate(reversed(parts)))
            cpu += seconds + (int(days) * 86400 if separator else 0)
            total += int(fields[2])
            pids.append(int(fields[1]))
            states.add(fields[4][0])
        return {'rssMiB': total / 1024, 'cpuSeconds': round(cpu, 3),
                'processes': len(pids), 'states': sorted(states), '_pids': tuple(sorted(pids))}

    @staticmethod
    def group_rss_mib(pgid):
        return Runner.group_activity(pgid)['rssMiB']

    def run(self, stage, args, cwd, env, deadline_minutes, log_limit=None):
        require(stage.replace('-', '').isalnum(), 'stage-name')
        require(deadline_minutes == self.limits['jobMinutes'], 'platform-budget-binding')
        path = self.task / (stage + '.private.log')
        limit = log_limit or self.limits['maxStageLogBytes']
        reason = None
        process = None
        progress = ProgressWatch(self.limits['stallQuietMinutes'])
        peak_rss, minimum_free, last_activity, quiet = 0, None, None, 0
        try:
            with path.open('xb') as output:
                process = subprocess.Popen(args, cwd=cwd, env=env, stdout=output,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                self.groups.add(process.pid)
                next_status = 0
                while process.poll() is None:
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
                    activity = self.group_activity(process.pid)
                    rss = activity['rssMiB']
                    if self.native_registry is not None:
                        self.native_registry.census()
                        rss += self.native_registry.outside_group_rss(process.pid)
                    quiet, stalled = progress.observe(time.monotonic(), size, free, activity)
                    peak_rss = max(peak_rss, rss)
                    minimum_free = free if minimum_free is None else min(minimum_free, free)
                    last_activity = {key: value for key, value in activity.items() if not key.startswith('_')}
                    if elapsed > deadline_minutes * 60:
                        reason = 'platform-job-limit'
                    elif free < self.limits['minFreeDuringBuildGiB'] * 1024**3:
                        reason = 'free-disk-reserve'
                    elif size > limit:
                        reason = 'private-log-limit'
                    elif private_bytes > self.limits['maxNativePrivateBytes']:
                        reason = 'native-private-size-limit'
                    elif rss > self.limits.get('maxOwnedRSSMiB', 10240):
                        reason = 'owned-process-memory-limit'
                    elif stalled:
                        reason = 'stalled-no-progress'
                    if reason:
                        self.fail(stage, reason)
                        self.stop(process)
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
            code = process.wait(timeout=5)
            if self.group_alive(process.pid):
                reason = reason or 'owned-descendants-outlived-leader'
                self.fail(stage, reason)
                self.stop(process)
            self.records.append({'stage': stage, 'exitCode': code, 'reason': reason,
                                 'logBytes': path.stat().st_size, 'peakRSSMiB': round(peak_rss),
                                 'minimumFreeGiB': round(minimum_free / 1024**3, 2) if minimum_free else None,
                                 'lastActivity': last_activity, 'quietSeconds': round(quiet)})
            if code or reason:
                self.fail(stage, reason or 'nonzero-exit')
                raise Failure(reason or 'nonzero-exit')
            return path
        except BaseException as error:
            self.fail(stage, str(error) if isinstance(error, Failure) else type(error).__name__)
            raise
        finally:
            if process is not None:
                try:
                    if self.group_alive(process.pid):
                        self.stop(process)
                finally:
                    self.groups.discard(process.pid)
                    if self.native_registry is not None:
                        self.native_cleanup = self.native_registry.finish()
                        if self.native_cleanup['outlivedHarness'] or not self.native_cleanup['allOwnedGone']:
                            self.fail(stage, 'owned-native-cleanup-incomplete')
                            raise Failure('owned-native-cleanup-incomplete')

    def stop(self, process):
        for value, grace in ((signal.SIGINT, self.limits['processCleanupGraceSeconds']),
                             (signal.SIGTERM, 10), (signal.SIGKILL, 5)):
            if not self.group_alive(process.pid):
                process.poll()
                return
            try:
                os.killpg(process.pid, value)
            except ProcessLookupError:
                break
            deadline = time.monotonic() + grace
            while self.group_alive(process.pid) and time.monotonic() < deadline:
                process.poll()
                time.sleep(0.05)
        require(not self.group_alive(process.pid), 'owned-process-cleanup-timeout')
