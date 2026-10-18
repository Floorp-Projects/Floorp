# SPDX-License-Identifier: MPL-2.0
import contextlib
import ctypes
import errno
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

from darwin_activity import BSDInfo, DarwinActivity
from monitor_ps_lifetime import child_status
from support import Failure, Runner


@contextlib.contextmanager
def owned_child():
    child = subprocess.Popen([sys.executable, '-c',
        "import sys; print('ready',flush=True); sys.stdin.buffer.read(1)"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
    try:
        if not select.select([child.stdout], [], [], 5)[0] or child.stdout.readline() != b'ready\n':
            raise RuntimeError('owned-child-not-ready')
        yield child
    finally:
        try:
            if child.poll() is None:
                child.stdin.write(b'x')
                child.stdin.flush()
        except BrokenPipeError:
            pass
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=5)
        finally:
            for stream in (child.stdin, child.stdout, child.stderr):
                stream.close()


@unittest.skipUnless(sys.platform == 'darwin', 'requires actual macOS process ABI')
class MacGenerationRecheckTests(unittest.TestCase):
    def test_real_live_generation_rechecked_after_controlled_esrch(self):
        sensor = DarwinActivity()
        with owned_child() as child:
            state, birth, _ = sensor.bsd(child.pid)
            self.assertEqual(state, 'measured')
            original = sensor.query
            injected = False
            def query(pid, flavor, value):
                nonlocal injected
                if pid == child.pid and flavor == 2 and not injected:
                    injected = True
                    return 0, errno.ESRCH
                return original(pid, flavor, value)
            with mock.patch.object(sensor, 'query', side_effect=query):
                value = sensor.read(child.pid, child.pid)
            self.assertEqual(value['status'], 'measured')
            self.assertGreater(value['rssMiB'], 0)
            self.assertEqual(value['_birth'], (child.pid, birth.start_sec, birth.start_usec))
            self.assertTrue(value['esrchRecheck']['sameGeneration'])
            print(json.dumps({'macGenerationGate': {'case': 'controlled-esrch-live',
                'actualKernelQueries': True, 'initialESRCHInjected': True,
                'sameGeneration': True, 'result': 'measured'}}), flush=True)
        self.assertEqual(child.returncode, 0)

    def test_real_same_birth_zombie_after_initial_live_bsd(self):
        sensor = DarwinActivity()
        with owned_child() as child:
            original = sensor.query
            injected = released = False
            def query(pid, flavor, value):
                nonlocal injected, released
                if pid == child.pid and flavor == 2 and not injected:
                    injected = True
                    return 0, errno.ESRCH
                size, code = original(pid, flavor, value)
                if pid == child.pid and flavor == 3 and not released:
                    self.assertEqual(size, ctypes.sizeof(BSDInfo))
                    self.assertIn(value.status, (1, 2, 3, 4))
                    child.stdin.write(b'x')
                    child.stdin.flush()
                    deadline = time.monotonic() + 5
                    while True:
                        info = child_status(child.pid)
                        self.assertTrue(info['matchingChild'])
                        if info['exitPending']:
                            break
                        self.assertLess(time.monotonic(), deadline)
                        time.sleep(0.005)
                    released = True
                return size, code
            with mock.patch.object(sensor, 'query', side_effect=query):
                value = sensor.read(child.pid, child.pid)
            self.assertEqual((value['status'], value['reason']), ('gone', 'bsd-zombie'))
            self.assertTrue(value['esrchRecheck']['sameGeneration'])
            self.assertNotIn('rssMiB', value)
            os.kill(child.pid, 0)
            print(json.dumps({'macGenerationGate': {'case': 'controlled-esrch-owned-zombie',
                'actualKernelQueries': True, 'initialESRCHInjected': True,
                'sameGeneration': True, 'WNOWAITExitPending': True, 'signalZeroSucceeded': True,
                'result': 'authoritative-zombie'}}), flush=True)
        self.assertEqual(child.returncode, 0)

    def test_natural_short_lived_owned_process_churn(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        observations = {'samples': 0, 'complete': 0, 'esrchRechecks': 0}
        original = Runner.group_activity
        def sample(pgid):
            value = original(pgid)
            observations['samples'] += 1
            observations['complete'] += int(value['sampleComplete'])
            observations['esrchRechecks'] += value.get('esrchRechecks', 0)
            return value
        script = """import os,time
for _ in range(240):
 children=[]
 for _ in range(3):
  pid=os.fork()
  if pid==0:
   time.sleep(0.02)
   os._exit(0)
  children.append(pid)
 for pid in children: os.waitpid(pid,0)
"""
        with tempfile.TemporaryDirectory() as directory:
            task = Path(directory)
            runner = Runner(task, time.time(), limits)
            with mock.patch.object(Runner, 'group_activity', side_effect=sample):
                runner.run('natural-churn', [sys.executable, '-c', script], task, os.environ.copy(), 360)
            record = runner.records[-1]
            self.assertIsNone(runner.failure)
            self.assertIsNone(runner.monitor_failure)
            self.assertGreaterEqual(observations['complete'], 3)
            self.assertEqual(record['groupCleanup'], 'verified-gone')
            self.assertEqual(record['leaderReap'], {'status': 'reaped', 'exitCode': 0})
            print(json.dumps({'macGenerationGate': {'case': 'natural-owned-churn',
                **observations, 'forkedChildren': 720, 'initialESRCHInjected': False,
                'naturalRaceObserved': observations['esrchRechecks'] > 0,
                'groupCleanup': record['groupCleanup'], 'leaderReaped': True}}), flush=True)

    def test_real_clang_error_excerpts_emitted_before_and_after_cleanup(self):
        limits = json.loads(Path(__file__).with_name('plan.json').read_text())['limits']
        with tempfile.TemporaryDirectory() as directory:
            task = Path(directory)
            source = task / 'diagnostic.c'
            source.write_text('int main(void) { return NONPUBLIC_SENTINEL; }\n')
            runner = Runner(task, time.time(), limits)
            with self.assertRaisesRegex(Failure, 'nonzero-exit'):
                runner.run('actual-clang-error', ['/usr/bin/clang', '-fsyntax-only', str(source)],
                           task, os.environ.copy(), 360)
            self.assertIsNone(runner.monitor_failure)
            record = runner.records[-1]
            self.assertEqual(record['leaderReap']['status'], 'reaped')
            self.assertEqual(record['groupCleanup'], 'verified-gone')
            for phase in ('beforeCleanup', 'afterCleanup'):
                snapshot = record['failureEvidence'][phase]
                self.assertTrue(snapshot['available'])
                self.assertTrue(any(r.get('messageClass') == 'undeclared-identifier' for r in snapshot['records']))
                self.assertNotIn('NONPUBLIC_SENTINEL', json.dumps(snapshot))
            print(json.dumps({'macGenerationGate': {'case': 'actual-clang-durable-excerpts',
                'realCompilerError': True, 'beforeAndAfterPresent': True,
                'freeTextPublished': False, 'leaderReaped': True}}), flush=True)


if __name__ == '__main__':
    unittest.main()
