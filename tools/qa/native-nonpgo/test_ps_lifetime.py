# SPDX-License-Identifier: MPL-2.0
import copy
import errno
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

from monitor_ps_lifetime import bounded_drain, child_status, publish, triple_denied, validate_held, wait_file
from support import Failure, write_json


class PsLifetimeGuards(unittest.TestCase):
    def test_denial_requires_exact_three_actual_eperm_queries(self):
        row = {'query': {'status': 'unavailable', 'taskQuery': {'bytes': 0, 'errno': errno.EPERM}}}
        self.assertTrue(triple_denied([row, row, row]))
        for rows in ([], [row], [row] * 2, [row] * 4,
                     [row, row, {'query': {'status': 'gone'}}],
                     [row, row, {'query': {'status': 'unavailable',
                                           'taskQuery': {'bytes': 0, 'errno': errno.EACCES}}}]):
            self.assertFalse(triple_denied(rows))

    def test_validation_requires_lifetime_generation_live_state_and_null_totals(self):
        identity = {'identityTag': 'known-birth', 'state': 2,
                    'parentMatchesWorker': True, 'groupMatchesTarget': True}
        ack = {'samePopenChild': True, 'matchingChild': True, 'noReapingWaitOrPoll': True,
               'exitPending': False, 'errno': 0}
        case = {'parentPopenLifetimeBinding': True, 'bsdSameIdentityAllObservations': True,
                'sameChildThreeEPERM': True, 'errorCode': 'monitor-unavailable',
                'firstFailure': {'code': 'monitor-unavailable'},
                'monitorFailure': {'diagnostic': True},
                'observations': [{'bsdBefore': identity, 'bsdAfter': identity,
                                  'parentBefore': ack, 'parentAfter': ack}] * 3,
                'samples': [{'sampleComplete': False, 'rssMiB': None, 'cpuSeconds': None}] * 3,
                'childReap': {'reaped': True, 'exitCode': 0, 'outputBytes': 131072},
                'stages': [{'groupCleanup': 'verified-gone', 'leaderReap': {'status': 'reaped'}}]}
        validate_held(case)
        for key in ('parentPopenLifetimeBinding',):
            bad = copy.deepcopy(case)
            bad[key] = False
            with self.assertRaises(Failure):
                validate_held(bad)
        bad = copy.deepcopy(case)
        bad['observations'][0]['parentAfter']['exitPending'] = True
        with self.assertRaises(Failure):
            validate_held(bad)
        bad = copy.deepcopy(case)
        bad['observations'][0]['bsdAfter'] = {**identity, 'identityTag': 'other-birth'}
        with self.assertRaises(Failure):
            validate_held(bad)
        bad = copy.deepcopy(case)
        bad['samples'][0]['rssMiB'] = 0
        with self.assertRaises(Failure):
            validate_held(bad)
        unknown = copy.deepcopy(case)
        unknown['bsdSameIdentityAllObservations'] = False
        for row in unknown['observations']:
            row['bsdBefore'] = row['bsdAfter'] = {'status': 'unavailable', 'reason': 'permission'}
        validate_held(unknown)
        failed_cleanup = copy.deepcopy(case)
        failed_cleanup['errorCode'] = 'PermissionError'
        failed_cleanup['stages'][0]['groupCleanup'] = 'unverified'
        with self.assertRaisesRegex(Failure, 'ps-group-cleanup-incomplete'):
            validate_held(failed_cleanup)

    def test_waitid_keeps_exact_exited_child_waitable_until_reaped(self):
        child = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.read(1); raise SystemExit(7)'],
                                 stdin=subprocess.PIPE)
        try:
            self.assertFalse(child_status(child.pid)['exitPending'])
            child.stdin.write(b'x')
            child.stdin.close()
            deadline = time.monotonic() + 5
            while not child_status(child.pid)['exitPending']:
                self.assertLess(time.monotonic(), deadline)
                time.sleep(.01)
            self.assertTrue(child_status(child.pid)['matchingChild'])
            self.assertTrue(child_status(child.pid)['exitPending'])
            self.assertEqual(child.wait(timeout=5), 7)
            with self.assertRaises(ChildProcessError):
                child_status(child.pid)
        finally:
            child.stdin.close()
            child.wait(timeout=5)

    def test_streaming_drain_rejects_overflow_and_closes_exact_child(self):
        child = subprocess.Popen([sys.executable, '-c', 'import os; os.write(1,b"x"*131072)'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            with self.assertRaises(Failure):
                bounded_drain(child, (1024, 1024))
        finally:
            child.stdout.close()
            child.stderr.close()
            child.wait(timeout=5)

    @unittest.skipUnless(os.name == 'posix', 'POSIX process ownership')
    def test_exact_worker_child_is_held_then_drained_and_reaped(self):
        with tempfile.TemporaryDirectory() as folder:
            task = Path(folder)
            worker = subprocess.Popen([sys.executable, str(Path(__file__).with_name('monitor_ps_lifetime.py')),
                                       '--worker', 'held', '--task', str(task)],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
            try:
                ready = wait_file(task / 'ready.json')
                self.assertEqual(ready['worker'], worker.pid)
                self.assertEqual(ready['pgid'], worker.pid)
                self.assertNotEqual(os.getpgrp(), ready['pgid'])
                for sequence in range(1, 7):
                    publish(task / 'ack-request.json', {'sequence': sequence})
                    ack = wait_file(task / f'ack-{sequence}.json')
                    self.assertEqual(ack['samePopenChild'], ready['pid'])
                    self.assertEqual(ack['sequence'], sequence)
                    self.assertTrue(ack['matchingChild'])
                    self.assertFalse(ack['exitPending'])
                    self.assertEqual(ack['errno'], 0)
                    time.sleep(.085)
                self.assertFalse((task / 'drained.json').exists())
                write_json(task / 'release.json', {'release': True})
                result = wait_file(task / 'drained.json')
                self.assertTrue(result['reaped'])
                self.assertEqual(result['exitCode'], 0)
                self.assertGreaterEqual(result['outputBytes'], 65536)
            finally:
                write_json(task / 'release.json', {'release': True})
                try:
                    os.kill(worker.pid, 2)
                except ProcessLookupError:
                    pass
                worker.communicate(timeout=10)
                worker.stdout.close()
                worker.stderr.close()
            self.assertEqual(worker.returncode, 0)


if __name__ == '__main__':
    unittest.main()
