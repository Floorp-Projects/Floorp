# SPDX-License-Identifier: MPL-2.0
import ctypes
import errno
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import unittest
from types import SimpleNamespace
from unittest import mock

from native_processes import BSDInfo, NativeProcesses, decode_argv
from monitor_ps_lifetime import child_status


class NativeInventoryGuards(unittest.TestCase):
    def inventory(self):
        lib = SimpleNamespace(**{name: mock.Mock() for name in
                                ('proc_listpids', 'proc_pidinfo', 'proc_pidpath')})
        system = SimpleNamespace(sysctl=mock.Mock())
        return NativeProcesses(lib, system)

    def test_argv_decodes_exact_argc_and_ignores_environment(self):
        data = struct.pack('=i', 3) + b'/a\0\0\0/a\0\0last\0PRIVATE=do-not-publish\0'
        self.assertEqual(decode_argv(data), ['/a', '', 'last'])
        for raw in (b'', struct.pack('=i', 0) + b'/a\0x\0', data[:15], b'X' * 65537):
            with self.assertRaises(RuntimeError):
                decode_argv(raw)

    def test_inventory_zero_errno_truncation_and_duplicates_are_unavailable(self):
        value = self.inventory()
        def listing(kind, uid, buffer, size):
            self.assertEqual((kind, uid), (4, os.geteuid()))
            buffer[0], buffer[1] = 900, 901
            return 8
        value.lib.proc_listpids.side_effect = listing
        self.assertEqual(value.pids(), [900, 901])
        for count in (0, -1, 3, 16384):
            value.lib.proc_listpids.side_effect = lambda *_args, count=count: count
            with self.assertRaises(RuntimeError):
                value.pids()
        def denied(*args):
            ctypes.set_errno(errno.EPERM)
            return 0
        value.lib.proc_listpids.side_effect = denied
        with self.assertRaises(RuntimeError):
            value.pids()
        def duplicate(kind, uid, buffer, size):
            buffer[0] = buffer[1] = 900
            return 8
        value.lib.proc_listpids.side_effect = duplicate
        with self.assertRaisesRegex(RuntimeError, 'native-process-list-invalid'):
            value.pids()

    def test_bsd_denial_and_short_identity_never_become_zero_or_gone(self):
        value = self.inventory()
        def denied(pid, flavor, arg, buffer, size):
            self.assertEqual((flavor, arg, size), (3, 1, 136))
            ctypes.set_errno(errno.EPERM)
            return 0
        value.lib.proc_pidinfo.side_effect = denied
        with mock.patch('native_processes.os.kill') as probe:
            with self.assertRaisesRegex(RuntimeError, 'native-birth-unavailable'):
                value.bsd(900)
        probe.assert_not_called()
        value.lib.proc_pidinfo.side_effect = lambda *_: 7
        with mock.patch('native_processes.os.kill'):
            with self.assertRaises(RuntimeError):
                value.bsd(900)

    def test_argv_einval_does_not_imply_exit(self):
        value = self.inventory()
        def invalid(*args):
            ctypes.set_errno(errno.EINVAL)
            return -1
        value.system.sysctl.side_effect = invalid
        with self.assertRaisesRegex(RuntimeError, 'native-argv-unavailable'):
            value.argv(900)
        value.system.sysctl.side_effect = lambda *_args: 0
        with self.assertRaisesRegex(RuntimeError, 'native-argv-unavailable'):
            value.argv(900)

    def test_rows_recheck_birth_owner_path_and_argv_without_spawning(self):
        value = self.inventory()
        before = SimpleNamespace(pid=900, uid=os.geteuid(), start_sec=100, start_usec=7)
        value.bsd = mock.Mock(return_value=before)
        value.path = mock.Mock(return_value='/native-browser')
        value.argv = mock.Mock(return_value=['/native-browser', '--profile', '/private-profile'])
        with mock.patch('subprocess.Popen', side_effect=AssertionError('unexpected helper')):
            self.assertEqual(value.rows(900), [(900, '100.000007', ('/native-browser', '--profile', '/private-profile'))])
        changed = SimpleNamespace(**{**vars(before), 'start_usec': 8})
        value.bsd.side_effect = [before, changed, changed]
        with self.assertRaisesRegex(RuntimeError, 'native-process-identity-changed'):
            value.rows(900)
        value.bsd.side_effect = None
        value.argv.side_effect = [['/native-browser'], ['/different-argv']]
        with self.assertRaisesRegex(RuntimeError, 'native-process-identity-changed'):
            value.rows(900)
        value.argv.side_effect = None
        value.path.side_effect = ['/native-browser', '/different-executable']
        with self.assertRaisesRegex(RuntimeError, 'native-process-identity-changed'):
            value.rows(900)

    def test_candidate_mismatch_exit_and_unavailable_are_distinct(self):
        value = self.inventory()
        birth = SimpleNamespace(pid=900, uid=os.geteuid(), start_sec=100, start_usec=7)
        value.pids = mock.Mock(return_value=[900])
        value.bsd = mock.Mock(return_value=birth)
        value.path = mock.Mock(return_value='/other-executable')
        value.argv = mock.Mock()
        self.assertEqual(value.rows(executable='/native-browser'), [])
        value.argv.assert_not_called()
        value.path.side_effect = RuntimeError('native-path-unavailable')
        with self.assertRaisesRegex(RuntimeError, 'native-path-unavailable'):
            value.rows(900)
        value.bsd.side_effect = [birth, None]
        self.assertEqual(value.rows(900), [])

    @unittest.skipUnless(sys.platform == 'darwin', 'real normal-permission Darwin process APIs')
    def test_real_mac_child_identity_and_argc_without_environment_or_ps(self):
        command = [sys.executable, '-c', 'import sys; sys.stdin.buffer.read(1)']
        child = subprocess.Popen(command, stdin=subprocess.PIPE,
                                 env={**os.environ, 'FLOORP_TEST_PRIVATE_ENV': 'never-in-process-rows'})
        try:
            inventory = NativeProcesses()
            self.assertIn(child.pid, inventory.pids())
            with mock.patch('subprocess.Popen', side_effect=AssertionError('unexpected ps helper')):
                rows = inventory.rows(child.pid)
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0][0], child.pid)
            self.assertEqual(rows[0][2], tuple(command))
            self.assertNotIn('never-in-process-rows', rows[0][2])
            self.assertRegex(rows[0][1], r'^[1-9][0-9]*\.[0-9]{6}$')
            path = inventory.path(child.pid)
            self.assertIn(rows[0], inventory.rows(executable=path))
            child.stdin.write(b'x')
            child.stdin.close()
            deadline = time.monotonic() + 5
            while not child_status(child.pid)['exitPending']:
                self.assertLess(time.monotonic(), deadline)
                time.sleep(.01)
            self.assertEqual(inventory.rows(child.pid), [])
            print(json.dumps({'ordinaryNativeInventory': {'sameOwnedPID': True,
                  'fullBirthAvailable': True, 'exactArgvMatched': True,
                  'environmentExcluded': True, 'candidateMatched': True,
                  'validatedZombieNotLive': True, 'helperProcessesStarted': 0,
                  'privilegeChanges': False}}), flush=True)
        finally:
            child.stdin.close()
            child.wait(timeout=5)


if __name__ == '__main__':
    unittest.main()
