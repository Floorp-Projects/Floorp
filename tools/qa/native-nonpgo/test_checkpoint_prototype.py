# SPDX-License-Identifier: MPL-2.0
"""Local design experiment only; no production pipeline or cloud entry point."""

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import signal
import stat
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
from unittest import mock

from failure_excerpt import classify, excerpt
from support import Failure, Runner, digest, require, scan_tar
from primary_anchor import PrimaryAnchor


DEADLINES_MINUTES = {'build': 240, 'cleanup': 245, 'scan': 260, 'archive': 305,
                     'upload': 335, 'receipt': 345, 'platform': 360}


def scan_full_log(path, length=None, stop_offset=None):
    result = {'scannedBytes': 0, 'interrupts': 0, 'blockingDiagnostics': 0,
              'makeError130AfterStop': 0}
    checksum = hashlib.sha256()
    prefix = hashlib.sha256()
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode), 'log-not-regular')
        count = info.st_size if length is None else length
        require(0 <= count <= info.st_size <= 256 * 1024**2, 'log-scan-bound')
        require(stop_offset is None or 0 <= stop_offset <= count, 'log-stop-offset')
        while count:
            line = stream.readline(min(count, 65537))
            require(bool(line), 'log-scan-short-read')
            require(len(line) <= 65536, 'log-line-unclassified')
            require(line.endswith(b'\n'), 'log-unterminated-line')
            count -= len(line)
            checksum.update(line)
            offset = result['scannedBytes']
            result['scannedBytes'] += len(line)
            if stop_offset is not None:
                require(not offset < stop_offset < result['scannedBytes'], 'log-stop-offset-mid-line')
                if result['scannedBytes'] <= stop_offset:
                    prefix.update(line)
            text = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', line.decode('utf-8', errors='strict'))
            make130 = re.fullmatch(r'make(?:\[[0-9]+\])?: \*\*\* \[[^\]\r\n]{1,4096}\] Error 130\s*', text)
            if make130 and stop_offset is not None and offset >= stop_offset:
                result['makeError130AfterStop'] += 1
                continue
            matches = classify(text)
            blocking = False
            for item in matches:
                if item.get('messageClass') in ('keyboard-interrupt', 'interrupted'):
                    result['interrupts'] += 1
                else:
                    blocking = True
            if blocking or re.search(r'\berror(?:\[[^\]]*\])?:|\berror [0-9]+\b|'
                                      r'\b(?:fatal|panic|panicked|failed|failure|assertion)\b', text, re.I):
                result['blockingDiagnostics'] += 1
        require(stream.tell() == result['scannedBytes'], 'log-scan-offset')
        final = os.fstat(stream.fileno())
        require(length is not None or (final.st_size == info.st_size and final.st_mtime_ns == info.st_mtime_ns),
                'log-changed-during-scan')
    return {**result, 'sha256': checksum.hexdigest(),
            'stopPrefixSHA256': prefix.hexdigest() if stop_offset is not None else None}


def interrupted_candidate(evidence, before, after):
    return (evidence['scheduled'] and evidence['aliveBeforeSignal'] and
            evidence['signals'] == [int(signal.SIGINT)] and
            evidence['groupGone'] and evidence['leaderReaped'] and
            evidence['firstFailure'] is None and evidence['monitorFailure'] is None and
            evidence['exitCode'] in (-int(signal.SIGINT), 130, 2) and
            before['blockingDiagnostics'] == after['blockingDiagnostics'] == 0 and
            after.get('stopPrefixSHA256') == before['sha256'] and
            after['interrupts'] > before['interrupts'])


class CappedOutput:
    def __init__(self, stream, cap, deadline):
        self.stream, self.cap, self.deadline, self.count = stream, cap, deadline, 0

    def write(self, data):
        require(time.monotonic() < self.deadline, 'archive-deadline')
        require(self.count + len(data) <= self.cap, 'archive-stored-cap')
        written = self.stream.write(data)
        require(written == len(data), 'archive-short-write')
        self.count += written
        return written

    def flush(self):
        self.stream.flush()


def object_rows(root, logical_cap, entry_cap):
    rows, logical = [], 0
    for path in sorted(Path(root).rglob('*')):
        info = path.lstat()
        require(stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode),
                'prototype-link-or-special-file')
        require(not stat.S_ISREG(info.st_mode) or info.st_nlink == 1, 'prototype-hardlink')
        require(info.st_uid == os.geteuid() and not info.st_mode & 0o7000,
                'object-owner-or-mode')
        require(len(rows) < entry_cap, 'object-entry-cap')
        logical += info.st_size if path.is_file() else 0
        require(logical <= logical_cap, 'object-logical-cap')
        rows.append({'path': str(path.relative_to(root)), 'directory': path.is_dir(),
                     'mode': stat.S_IMODE(info.st_mode), 'mtimeNS': info.st_mtime_ns,
                     'bytes': info.st_size if path.is_file() else 0,
                     'sha256': digest(path) if path.is_file() else None})
    return rows


def local_archive(obj, archive, *, stored_cap=1024**2, logical_cap=1024**2,
                  entry_cap=100, deadline=None):
    rows = object_rows(obj, logical_cap, entry_cap)
    created = False
    try:
        with Path(archive).open('xb') as stream:
            created = True
            capped = CappedOutput(stream, stored_cap, deadline or time.monotonic() + 10)
            with tarfile.open(fileobj=capped, mode='w|gz', format=tarfile.PAX_FORMAT) as output:
                output.add(obj, arcname='obj', recursive=True)
        require(rows == object_rows(obj, logical_cap, entry_cap), 'object-tree-changed')
    except BaseException:
        if created:
            Path(archive).unlink(missing_ok=True)
        raise
    return rows


def local_restore(archive, staging, rows, expected_hash, expected_context, context):
    require(context == expected_context, 'resume-cohort-mismatch')
    require(digest(archive) == expected_hash, 'resume-archive-hash')
    scan_tar(archive, max_bytes=1024**2, max_members=101)
    with tarfile.open(archive) as packed:
        require(all(PurePosixPath(item.name).parts[0] == 'obj' for item in packed),
                'resume-root-not-object')
        Path(staging).mkdir(mode=0o700, exist_ok=False)
        packed.extractall(staging, filter='data')
    obj = Path(staging) / 'obj'
    actual = object_rows(obj, 1024**2, 100)
    require([{key: value for key, value in row.items() if key != 'mtimeNS'} for row in actual] ==
            [{key: value for key, value in row.items() if key != 'mtimeNS'} for row in rows],
            'resume-object-content')
    for row in reversed(rows):
        name = PurePosixPath(row['path'])
        require(not name.is_absolute() and '..' not in name.parts, 'resume-manifest-path')
        os.utime(obj / row['path'], ns=(row['mtimeNS'], row['mtimeNS']), follow_symlinks=False)
    require(object_rows(obj, 1024**2, 100) == rows, 'resume-object-metadata')
    return obj


class CheckpointPrototypeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='floorp-checkpoint-local-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.log = self.root / 'compile.private.log'

    def evidence(self):
        return {'scheduled': True, 'aliveBeforeSignal': True, 'signals': [int(signal.SIGINT)],
                'groupGone': True, 'leaderReaped': True, 'firstFailure': None,
                'monitorFailure': None, 'exitCode': -int(signal.SIGINT)}

    def test_fixed_windows_leave_fifteen_minutes_before_platform_limit(self):
        self.assertEqual(list(DEADLINES_MINUTES.values()), [240, 245, 260, 305, 335, 345, 360])
        self.assertGreaterEqual((245 - 240) * 60, 90 + 10 + 5 + 5 + 10)
        self.assertEqual(360 - 345, 15)

    def test_full_scan_finds_early_error_outside_prior_tail(self):
        self.log.write_bytes(b"x.cc:1: error: use of undeclared identifier 'PRIVATE'\n" +
                             b'ordinary progress\n' * 20000)
        before = scan_full_log(self.log)
        with self.log.open('ab') as output:
            output.write(b'Interrupt: 2\n')
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertEqual(before['scannedBytes'], self.log.stat().st_size - 13)
        self.assertFalse(interrupted_candidate(self.evidence(), before, after))
        self.assertNotIn('PRIVATE', json.dumps(after))

    def test_error_racing_with_sigint_is_rejected(self):
        self.log.write_text('ordinary progress\n')
        before = scan_full_log(self.log)
        with self.log.open('a') as output:
            output.write('error[E0308]: PRIVATE\nInterrupt: 2\n')
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertEqual(after['stopPrefixSHA256'], before['sha256'])
        self.assertGreater(after['blockingDiagnostics'], 0)
        self.assertFalse(interrupted_candidate(self.evidence(), before, after))

    def test_unknown_make_error_is_not_excused_by_interrupt(self):
        self.log.write_text('ordinary progress\n')
        before = scan_full_log(self.log)
        with self.log.open('a') as output:
            output.write('make: *** [target] Error 2\nKeyboardInterrupt\n')
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertEqual(after['stopPrefixSHA256'], before['sha256'])
        self.assertGreater(after['blockingDiagnostics'], 0)
        self.assertFalse(interrupted_candidate(self.evidence(), before, after))

    def test_interleaved_interrupt_does_not_hide_same_line_error_or_fatal(self):
        for line in ('make: *** [target] Error 2 KeyboardInterrupt\n',
                     'fatal: failed assertion interrupted by user\n',
                     'error[E0308]: PRIVATE KeyboardInterrupt\n'):
            with self.subTest(line=line):
                self.log.write_text('ordinary progress\n')
                before = scan_full_log(self.log)
                with self.log.open('a') as stream:
                    stream.write(line)
                after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
                self.assertEqual(after['stopPrefixSHA256'], before['sha256'])
                self.assertGreater(after['interrupts'], 0)
                self.assertGreater(after['blockingDiagnostics'], 0)
                self.assertFalse(interrupted_candidate(self.evidence(), before, after))

    def test_make130_is_candidate_only_after_recorded_stop_offset(self):
        self.log.write_text('ordinary progress\n')
        before = scan_full_log(self.log)
        with self.log.open('a') as output:
            output.write('interrupted by user\nmake: *** [Makefile:3: slow.o] Error 130\n')
        self.assertFalse(interrupted_candidate(self.evidence(), before, scan_full_log(self.log)))
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertEqual(after['makeError130AfterStop'], 1)
        self.assertTrue(interrupted_candidate(self.evidence(), before, after))

    def test_unknown_fatal_is_blocking(self):
        self.log.write_text('fatal: private text\n')
        self.assertEqual(scan_full_log(self.log)['blockingDiagnostics'], 1)

    def test_changed_stop_prefix_is_rejected(self):
        self.log.write_text('progress A\n')
        before = scan_full_log(self.log)
        self.log.write_text('progress B\nInterrupt: 2\n')
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertFalse(interrupted_candidate(self.evidence(), before, after))

    def test_nonzero_needs_signal_cleanup_and_no_prior_failure(self):
        self.log.write_text('ordinary progress\n')
        before = scan_full_log(self.log)
        with self.log.open('a') as output:
            output.write('Interrupt: 2\n')
        after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
        self.assertTrue(interrupted_candidate(self.evidence(), before, after))
        for key, value in (('scheduled', False), ('aliveBeforeSignal', False),
                           ('signals', [2, 15]), ('signals', [2, 15, 9]),
                           ('groupGone', False), ('leaderReaped', False),
                           ('firstFailure', {'code': 'nonzero-exit'}),
                           ('monitorFailure', {'status': 'unknown'}), ('exitCode', 1)):
            with self.subTest(key=key, value=value):
                self.assertFalse(interrupted_candidate({**self.evidence(), key: value}, before, after))

    def test_oversized_line_is_unknown(self):
        self.log.write_bytes(b'x' * 65537)
        with self.assertRaisesRegex(Failure, 'log-line-unclassified'):
            scan_full_log(self.log)

    def test_unterminated_line_and_midline_stop_offset_are_unknown(self):
        self.log.write_bytes(b'partial diagnostic')
        with self.assertRaisesRegex(Failure, 'log-unterminated-line'):
            scan_full_log(self.log)
        self.log.write_text('complete line\n')
        with self.assertRaisesRegex(Failure, 'log-stop-offset-mid-line'):
            scan_full_log(self.log, stop_offset=3)

    def test_log_growth_during_final_scan_is_unknown(self):
        self.log.write_text('progress\n')
        original = classify
        def append_during_scan(text):
            with self.log.open('a') as stream:
                stream.write('late progress\n')
            return original(text)
        with mock.patch(__name__ + '.classify', side_effect=append_during_scan):
            with self.assertRaisesRegex(Failure, 'log-changed-during-scan'):
                scan_full_log(self.log)

    def test_log_symlink_is_not_read(self):
        self.log.symlink_to(self.root / 'absent')
        with self.assertRaises(OSError):
            scan_full_log(self.log)

    def test_archive_caps_and_deadline_leave_no_partial_archive(self):
        obj = self.root / 'obj'
        obj.mkdir()
        (obj / 'data').write_bytes(os.urandom(4096))
        archive = self.root / 'checkpoint.tar.gz'
        for kwargs, error in (({'stored_cap': 128}, 'archive-stored-cap'),
                              ({'logical_cap': 128}, 'object-logical-cap'),
                              ({'entry_cap': 0}, 'object-entry-cap'),
                              ({'deadline': time.monotonic() - 1}, 'archive-deadline')):
            with self.subTest(error=error), self.assertRaisesRegex(Failure, error):
                local_archive(obj, archive, **kwargs)
            self.assertFalse(archive.exists())

    def test_external_link_and_fifo_are_not_packaged(self):
        obj = self.root / 'obj'
        obj.mkdir()
        path = obj / 'escape'
        path.symlink_to('../started-monotonic')
        with self.assertRaisesRegex(Failure, 'prototype-link-or-special-file'):
            local_archive(obj, self.root / 'link.tar.gz')
        path.unlink()
        os.mkfifo(path)
        with self.assertRaisesRegex(Failure, 'prototype-link-or-special-file'):
            local_archive(obj, self.root / 'fifo.tar.gz')

    def test_hardlink_is_not_packaged(self):
        obj = self.root / 'obj'
        obj.mkdir()
        original = self.root / 'outside'
        original.write_bytes(b'outside fixture')
        os.link(original, obj / 'alias')
        with self.assertRaisesRegex(Failure, 'prototype-hardlink'):
            local_archive(obj, self.root / 'hardlink.tar.gz')

    def test_existing_archive_is_preserved(self):
        obj = self.root / 'obj'
        obj.mkdir()
        archive = self.root / 'existing.tar.gz'
        archive.write_bytes(b'preserve existing fixture')
        with self.assertRaises(FileExistsError):
            local_archive(obj, archive)
        self.assertEqual(archive.read_bytes(), b'preserve existing fixture')

    def test_restore_rejects_changed_source_sdk_root_and_build_identity(self):
        obj = self.root / 'obj'
        obj.mkdir()
        (obj / 'object').write_bytes(b'completed object fixture')
        archive = self.root / 'checkpoint.tar.gz'
        rows = local_archive(obj, archive)
        context = {'sourceSHA256': 'source-A', 'sdkTreeSHA256': 'sdk-A',
                   'objRoot': str(obj), 'buildDate': '20261018000000'}
        for key in context:
            with self.subTest(key=key), self.assertRaisesRegex(Failure, 'resume-cohort-mismatch'):
                local_restore(archive, self.root / 'staging', rows, digest(archive), context,
                              {**context, key: 'changed'})
            self.assertFalse((self.root / 'staging').exists())

    def test_restore_rejects_tampered_payload_before_extract(self):
        obj = self.root / 'obj'
        obj.mkdir()
        (obj / 'object').write_bytes(b'completed object fixture')
        archive = self.root / 'checkpoint.tar.gz'
        rows = local_archive(obj, archive)
        checksum = digest(archive)
        with archive.open('ab') as stream:
            stream.write(b'TAMPER')
        with self.assertRaisesRegex(Failure, 'resume-archive-hash'):
            local_restore(archive, self.root / 'staging', rows, checksum, {}, {})
        self.assertFalse((self.root / 'staging').exists())

    @unittest.skipUnless(shutil.which('make') and shutil.which('cc'), 'requires local make and C compiler')
    def test_real_make_interrupt_archive_restore_and_resume(self):
        source, obj = self.root / 'source', self.root / 'obj'
        source.mkdir()
        obj.mkdir()
        (source / 'stable.c').write_text('int stable(void) { return 7; }\n')
        (source / 'slow.c').write_text('int stable(void); int main(void) { return stable() != 7; }\n')
        worker = source / 'slow.py'
        worker.write_text("import os, pathlib, signal, subprocess, sys, time\n"
                          "if os.environ.get('PAUSE_COMPILE') == '1':\n"
                          "    def stopped(*args):\n"
                          "        print('interrupted by user', flush=True)\n"
                          "        raise SystemExit(130)\n"
                          "    signal.signal(signal.SIGINT, stopped)\n"
                          "    pathlib.Path('slow.o').write_bytes(b'PARTIAL OBJECT')\n"
                          "    pathlib.Path('ready').touch()\n"
                          "    while True: time.sleep(.01)\n"
                          "subprocess.run(['cc', '-c', sys.argv[1], '-o', 'slow.o'], check=True)\n")
        (obj / 'Makefile').write_text(
            '.DELETE_ON_ERROR:\nall: program\n'
            f'stable.o: {source}/stable.c\n\tcc -c $< -o $@\n'
            f'slow.o: {source}/slow.c stable.o\n\t{sys.executable} {worker} $<\n'
            'program: stable.o slow.o\n\tcc stable.o slow.o -o program\n')
        (obj / '.fingerprint').mkdir()
        (obj / '.fingerprint/completed').write_text('toy hidden state\n')
        # Old private state must stay out of the archive, while this live
        # cleanup experiment still obeys the original platform deadline.
        (self.root / 'started-monotonic').write_text(str(time.monotonic() - 10) + '\n')
        (self.root / 'first-failure.json').write_text('{"code":"OLD-PRIVATE"}\n')
        env = {**os.environ, 'PAUSE_COMPILE': '1', 'LC_ALL': 'C'}
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        limits['processCleanupGraceSeconds'] = 3
        runner = Runner(self.root, time.time(), limits)
        anchor = None
        signals = []
        actual_killpg = os.killpg
        def observed_signal(pgid, value):
            if value:
                signals.append(int(value))
            return actual_killpg(pgid, value)
        try:
            with self.log.open('xb') as output:
                anchor = PrimaryAnchor.spawn(['make', '-j', '1'], cwd=obj, env=env, stdout=output,
                    stderr=subprocess.STDOUT, census=lambda pgid, *, deadline=None:
                    runner.anchor_inventory(pgid, 'local-checkpoint-interrupt', deadline=deadline))
                deadline = time.monotonic() + 10
                while not (obj / 'ready').exists() and time.monotonic() < deadline:
                    self.assertEqual(anchor.observe()['status'], 'live')
                    time.sleep(.01)
                self.assertTrue((obj / 'ready').exists())
                self.assertEqual(anchor.observe()['status'], 'live')
                self.assertEqual((obj / 'slow.o').read_bytes(), b'PARTIAL OBJECT')
                stable_hash, stable_mtime = digest(obj / 'stable.o'), (obj / 'stable.o').stat().st_mtime_ns
                before = scan_full_log(self.log)
                with mock.patch('support.os.killpg', side_effect=observed_signal):
                    runner.stop(anchor)
                code = anchor.actual_returncode
            self.assertEqual(anchor.lifecycle_state, 'reaped')
            self.assertTrue(anchor.post_reap_absence(deadline=time.monotonic() + 3)['groupEmpty'])
            with self.assertRaises(ChildProcessError):
                os.waitpid(anchor.pid, os.WNOHANG)
            self.assertEqual(signals, [int(signal.SIGINT)])
            self.assertFalse((obj / 'slow.o').exists())
            after = scan_full_log(self.log, stop_offset=before['scannedBytes'])
            self.assertTrue(interrupted_candidate({**self.evidence(), 'exitCode': code}, before, after),
                            json.dumps({'exitCode': code, 'before': before, 'after': after,
                                        'excerpt': excerpt(self.log)}))
            archive = self.root / 'checkpoint.tar.gz'
            rows = local_archive(obj, archive)
            archive_hash = digest(archive)
            self.assertGreater(scan_tar(archive)['members'], len(rows))
            shutil.rmtree(obj)
            self.assertEqual(digest(archive), archive_hash)
            context = {'sourceSHA256': digest(source / 'stable.c'), 'objRoot': str(obj),
                       'buildDate': '20261018000000'}
            restored = local_restore(archive, self.root / 'staging', rows, archive_hash, context, context)
            restored.rename(obj)
            self.assertEqual(digest(obj / 'stable.o'), stable_hash)
            self.assertEqual((obj / 'stable.o').stat().st_mtime_ns, stable_mtime)
            fresh_task = self.root / 'consumer-task'
            fresh_task.mkdir()
            (fresh_task / 'started-monotonic').write_text(str(time.monotonic()))
            fresh = Runner(fresh_task, time.time(), limits)
            self.assertIsNone(fresh.failure)
            self.assertEqual(fresh.groups, set())
            self.assertIsNone(fresh.native_registry)
            self.assertLess(fresh.elapsed(), 1)
            self.assertFalse((obj / 'started-monotonic').exists())
            self.assertFalse((obj / 'first-failure.json').exists())
            env.pop('PAUSE_COMPILE')
            result = subprocess.run(['make', '-j', '1'], cwd=obj, env=env,
                                    capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(digest(obj / 'stable.o'), stable_hash)
            self.assertEqual((obj / 'stable.o').stat().st_mtime_ns, stable_mtime)
            self.assertEqual(subprocess.run([str(obj / 'program')], timeout=5).returncode, 0)
            print(json.dumps({'localCheckpointExperiment': {'scheduledSignal': 'SIGINT',
                'interruptedExit': code, 'groupCleanup': 'verified-gone', 'leaderReaped': True,
                'partialTargetDeleted': True, 'completedObjectReused': True,
                'archiveBytes': archive.stat().st_size, 'archiveSHA256': archive_hash,
                'beforeLog': before, 'afterLog': after, 'consumerBuildExit': result.returncode,
                'floorpQualification': False}}), flush=True)
        finally:
            if anchor is not None and anchor.lifecycle_state != 'reaped':
                runner.stop(anchor)
            if anchor is not None:
                self.assertIsNotNone(anchor.actual_returncode)


if __name__ == '__main__':
    unittest.main()
