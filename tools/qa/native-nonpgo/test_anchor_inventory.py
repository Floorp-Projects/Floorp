# SPDX-License-Identifier: MPL-2.0
"""Portable producer/clock controls; these do not qualify native Mac census."""
import builtins
import errno
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest import mock

import anchor_inventory as inventory
import darwin_inventory
from process_census import BINDING_KEYS
from qa_core import Failure


GROUP = 1234
SECRET = 'PRIVATE_SENTINEL'


def environment():
    return {key: 'fixed-' + key for key in (*BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI')}


def measured(pids):
    return json.dumps({'status': 'measured', 'pids': pids, 'group': GROUP, 'queries': [],
        'measurement': {'api': 'sysctl-kern-proc-pgrp', 'snapshotBytes': len(pids)*648,
            'snapshotEntries': len(pids), 'selfBindingVerified': True, 'layoutSHA256': 'a'*64}})


def unavailable(**changes):
    value = {'status': 'unavailable', 'pids': None, 'group': GROUP, 'queries': [],
        'failureCode': 'inventory-query-permission', 'phase': 'group-data',
        'errorType': 'failure', 'errno': errno.EACCES}
    value.update(changes)
    return json.dumps(value)


class AnchorInventoryTests(unittest.TestCase):
    def call(self, *, platform='darwin', stdout=None, error=None, stage=None,
             deadline=None, clock=None, env=None, group=GROUP):
        with mock.patch.object(inventory.sys, 'platform', platform), \
             mock.patch.dict(inventory.os.environ, environment() if env is None else env, clear=True), \
             mock.patch.object(inventory.subprocess, 'run', side_effect=error,
                               return_value=SimpleNamespace(stdout=stdout)) as producer, \
             mock.patch.object(inventory.time, 'monotonic',
                               **({'return_value': 100.0} if clock is None else {'side_effect': clock})):
            try:
                value = inventory.collect(group, bootstrap_stage=stage, deadline=deadline)
                caught = None
            except Exception as caught_error:
                value, caught = None, caught_error
            calls = list(producer.call_args_list)
        return value, caught, calls

    def assert_private_error(self, error):
        self.assertIsInstance(error, inventory.InventoryError)
        public = json.dumps({'code': str(error), 'details': error.details})
        self.assertNotIn(SECRET, public)
        self.assertNotIn('stdout', public)
        self.assertNotIn('stderr', public)
        self.assertNotIn('argv', public)
        self.assertNotIn('message', public)

    def test_group_is_a_strict_positive_bounded_integer_before_any_producer(self):
        for group in (True, False, 0, -1, 2**31, 1.0, '1', None):
            with self.subTest(group=group):
                value, error, calls = self.call(group=group)
                self.assertIsNone(value); self.assertIsInstance(error, Failure)
                self.assertEqual(str(error), 'anchor-inventory-group')
                self.assertEqual(calls, [])

    def test_only_two_exact_bootstrap_stages_select_ps(self):
        for stage in ('census-abi-compile', 'census-abi'):
            with self.subTest(stage=stage):
                value, error, calls = self.call(stage=stage, stdout='0 0\n1234 2345\n1234 1234\n')
                self.assertIsNone(error)
                self.assertEqual(value['pids'], [1234, 2345])
                self.assertEqual(value['source'], 'darwin-bootstrap-ps')
                self.assertEqual(calls[0].args[0], ['/bin/ps', '-axo', 'pgid=,pid='])
                self.assertEqual(calls[0].kwargs['timeout'], 3.0)
        for stage in ('checkpoint-restore', 'census-abi-stale', '', True, 1):
            with self.subTest(stage=stage):
                value, error, calls = self.call(stage=stage)
                self.assertIsNone(value)
                self.assertEqual(str(error), 'anchor-inventory-bootstrap-stage')
                self.assertEqual(calls, [])

    def test_unsupported_platform_does_not_launch(self):
        value, error, calls = self.call(platform='unsupported')
        self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-platform')
        self.assertEqual(calls, [])

    def test_raw_zombie_members_survive_decoder_and_native_adapter_without_live_filter(self):
        fields = {'pid': (0, 4, True), 'startSec': (8, 8, True), 'startUsec': (16, 8, True),
                  'state': (24, 1, True), 'uid': (28, 4, False), 'ruid': (32, 4, False)}
        layout = {'schema': 1, 'pointerBytes': 8, 'recordBytes': 256, 'byteOrder': 'little',
                  'mib': [1, 14, 0, 0], 'fields': {
                      key: {'offset': offset, 'bytes': size, 'signed': signed}
                      for key, (offset, size, signed) in fields.items()}}
        raw = bytearray()
        for pid, state in ((1234, 2), (2345, 5)):
            record = bytearray(256)
            # Raw group membership also retains other owners; later bound
            # identity checks, not this adapter, decide signal authority.
            values = {'pid': pid, 'startSec': 10, 'startUsec': 2, 'state': state,
                      'uid': 0 if state == 5 else 501, 'ruid': 0 if state == 5 else 501}
            for key, (offset, size, signed) in fields.items():
                record[offset:offset+size] = values[key].to_bytes(size, 'little', signed=signed)
            raw.extend(record)
        pids = darwin_inventory.pids_from_records(bytes(raw), layout)
        self.assertEqual(pids, [1234, 2345])
        value, error, calls = self.call(stdout=measured(pids))
        self.assertIsNone(error); self.assertEqual(value['pids'], pids)
        self.assertEqual(value['status'], 'complete'); self.assertTrue(value['bindingVerified'])
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0].args[0][0], sys.executable)
        self.assertEqual(Path(calls[0].args[0][1]).name, 'darwin_inventory.py')
        self.assertEqual(calls[0].args[0][-2:], ['--pgid', str(GROUP)])

    def test_native_positive_empty_snapshot_is_distinct_from_unavailable(self):
        value, error, calls = self.call(stdout=measured([]))
        self.assertIsNone(error); self.assertEqual(value['pids'], [])
        self.assertEqual(value['status'], 'complete'); self.assertEqual(len(calls), 1)
        value, error, calls = self.call(stdout=unavailable())
        self.assertIsNone(value); self.assert_private_error(error)
        self.assertEqual(str(error), 'anchor-inventory-unavailable')
        self.assertEqual(error.details['errno'], errno.EACCES)
        self.assertEqual(len(calls), 1)
        self.assertNotEqual(calls[0].args[0][0], '/bin/ps')
        value, error, calls = self.call(stdout=unavailable(
            failureCode='inventory-abi-unavailable', phase='abi-binding'))
        self.assertIsNone(value); self.assert_private_error(error)
        self.assertEqual(error.details['failureCode'], 'inventory-abi-unavailable')
        self.assertEqual(len(calls), 1)
        self.assertNotEqual(calls[0].args[0][0], '/bin/ps')

    def test_native_invalid_pid_roster_never_becomes_complete_or_ps_fallback(self):
        for pids in ([True], [0], [-1], [2**31], [1234, 1234]):
            with self.subTest(pids=pids):
                value, error, calls = self.call(stdout=measured(pids))
                self.assertIsNone(value); self.assert_private_error(error)
                self.assertEqual(str(error), 'inventory-protocol-pids')
                self.assertEqual(len(calls), 1)
                self.assertNotEqual(calls[0].args[0][0], '/bin/ps')

    def test_malformed_native_protocol_and_bool_errno_are_fixed_private_failures(self):
        valid = json.loads(measured([1234]))
        variants = ['', '{', '{}', unavailable(errno=True)]
        for key, bad in (('status', True), ('group', True), ('PRIVATE', SECRET)):
            value = dict(valid); value[key] = bad; variants.append(json.dumps(value))
        for text in variants:
            with self.subTest(text=text):
                value, error, calls = self.call(stdout=text)
                self.assertIsNone(value); self.assert_private_error(error)
                self.assertEqual(len(calls), 1)

    def test_every_required_binding_missing_blocks_native_launch(self):
        for missing in (*BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI'):
            env = environment(); del env[missing]
            with self.subTest(missing=missing):
                value, error, calls = self.call(env=env)
                self.assertIsNone(value); self.assert_private_error(error)
                self.assertEqual(str(error), 'anchor-inventory-abi-missing')
                self.assertEqual(calls, [])

    def test_native_environment_preserves_bindings_but_excludes_tokens_and_import_overrides(self):
        env = environment()
        env.update({key: SECRET for key in ('GITHUB_TOKEN', 'PRIVATE_SECRET', 'PYTHONPATH', 'PATH')})
        env.update(HOME='fixed-home', TMPDIR='fixed-tmp', LANG='C', LC_ALL='C')
        value, error, calls = self.call(stdout=measured([1234]), env=env)
        self.assertIsNone(error)
        actual = calls[0].kwargs['env']
        self.assertEqual(set(actual), set(environment()) | {'HOME', 'TMPDIR', 'LANG', 'LC_ALL', 'PYTHONDONTWRITEBYTECODE'})
        self.assertEqual(actual['PYTHONDONTWRITEBYTECODE'], '1')
        self.assertNotIn(SECRET, json.dumps(actual))
        for key, expected in environment().items(): self.assertEqual(actual[key], expected)
        self.assertTrue(calls[0].kwargs['capture_output']); self.assertTrue(calls[0].kwargs['check'])
        self.assertTrue(calls[0].kwargs['text'])

    def test_producer_failures_keep_only_fixed_code_details_without_native_fallback(self):
        errors = (
            subprocess.TimeoutExpired([SECRET], 3, output=SECRET, stderr=SECRET),
            subprocess.CalledProcessError(7, [SECRET], output=SECRET, stderr=SECRET),
            PermissionError(errno.EACCES, SECRET), TypeError(SECRET), Failure(SECRET))
        for injected in errors:
            with self.subTest(kind=type(injected).__name__):
                value, error, calls = self.call(error=injected)
                self.assertIsNone(value); self.assert_private_error(error)
                self.assertEqual(len(calls), 1)
                self.assertNotEqual(calls[0].args[0][0], '/bin/ps')

    def test_only_fixed_validation_failure_codes_are_preserved(self):
        for code in ('anchor-inventory-deadline', 'anchor-inventory-schema', 'inventory-protocol-size'):
            value, error, calls = self.call(error=Failure(code))
            self.assertIsNone(value); self.assertEqual(str(error), code)
            self.assertEqual(error.details['errorType'], 'validation'); self.assertEqual(len(calls), 1)
        for injected in (Failure(SECRET), Failure('anchor-inventory-' + SECRET), Failure(),
                         Failure(True), Failure('anchor-inventory-schema', SECRET)):
            value, error, calls = self.call(error=injected)
            self.assertIsNone(value); self.assert_private_error(error)
            self.assertEqual(str(error), 'anchor-inventory-unavailable')

    def test_exception_errno_and_returncode_are_strict_ints_not_booleans(self):
        for code in (True, False, '7', None):
            value, error, calls = self.call(error=subprocess.CalledProcessError(code, [SECRET], output=SECRET))
            self.assertIsNone(value); self.assert_private_error(error)
            self.assertIsNone(error.details['exitCode'])
        for code in (True, False):
            value, error, calls = self.call(error=OSError(code, SECRET))
            self.assertIsNone(value); self.assert_private_error(error)
            self.assertIsNone(error.details['errno'])
        for injected, field, expected in ((OSError(errno.EACCES, SECRET), 'errno', errno.EACCES),
                (subprocess.CalledProcessError(-9, [SECRET]), 'exitCode', -9)):
            value, error, calls = self.call(error=injected)
            self.assertEqual(error.details[field], expected)

    def test_timeout_keeps_bounded_progress_without_raw_output(self):
        output = b'{"inventoryProgress":"project-imports","sequence":1}\n' + SECRET.encode()
        value, error, calls = self.call(error=subprocess.TimeoutExpired([SECRET], 3, output=output, stderr=SECRET))
        self.assertIsNone(value); self.assert_private_error(error)
        self.assertEqual(error.details['helperProgress'], {'phase': 'project-imports', 'events': 1})
        self.assertEqual(error.details['timeoutSeconds'], 3)
        self.assertEqual(str(error), 'anchor-inventory-timeout')

    def test_helper_timeout_cap_and_absolute_remaining_budget_after_setup(self):
        for deadline, clock, expected in ((None, None, 3.0), (200, [100, 101, 101], 3.0),
                                          (101, [100, 100.6, 100.7], .4)):
            with self.subTest(deadline=deadline):
                value, error, calls = self.call(stdout=measured([1234]), deadline=deadline, clock=clock)
                self.assertIsNone(error); self.assertEqual(value['status'], 'complete')
                self.assertAlmostEqual(calls[0].kwargs['timeout'], expected)

    def test_invalid_and_entrance_expired_deadlines_do_not_launch(self):
        for deadline in (True, False, float('nan'), float('inf'), -float('inf'), '101', 99):
            with self.subTest(deadline=deadline):
                value, error, calls = self.call(deadline=deadline)
                self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-deadline')
                self.assertEqual(calls, [])

    def test_import_preparation_crossing_absolute_deadline_prevents_native_launch(self):
        current = [100.0]
        original_import = builtins.__import__
        def delayed_import(name, *args, **kwargs):
            if name == 'darwin_inventory': current[0] = 102.0
            return original_import(name, *args, **kwargs)
        with mock.patch.object(builtins, '__import__', side_effect=delayed_import):
            value, error, calls = self.call(stdout=measured([1234]), deadline=101,
                                             clock=lambda: current[0])
        self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-deadline')
        self.assertEqual(calls, [])

    def test_ps_setup_deadline_expiry_prevents_launch(self):
        value, error, calls = self.call(platform='linux', stdout='0 0\n1234 1234\n',
                                        deadline=101, clock=[100, 102])
        self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-deadline')
        self.assertEqual(calls, [])

    def test_decoding_at_or_after_absolute_deadline_is_not_a_valid_snapshot(self):
        for platform, stdout in (('darwin', measured([1234])), ('linux', '0 0\n1234 1234\n')):
            with self.subTest(platform=platform):
                value, error, calls = self.call(platform=platform, stdout=stdout,
                                                deadline=101, clock=[100, 100, 101])
                self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-deadline')
                self.assertEqual(len(calls), 1)

    def test_ps_positive_global_snapshot_without_matching_group_can_be_empty(self):
        value, error, calls = self.call(platform='linux', stdout='0 0\n44 99\n')
        self.assertIsNone(error); self.assertEqual(value['pids'], [])
        self.assertEqual(value['source'], 'linux-ps-test'); self.assertEqual(value['status'], 'complete')
        self.assertEqual(len(calls), 1)

    def test_ps_rejects_empty_malformed_duplicate_or_invalid_global_rows(self):
        variants = ('', '\n', '0 0\n1234 1234\n77 1234\n', '0 0\n77 567\n' + SECRET,
                    '1 2 3', '1234 0', '77 22\n88 22\n', '-1 2', '2147483648 2', '2 2147483648')
        for stdout in variants:
            with self.subTest(stdout=stdout):
                value, error, calls = self.call(platform='linux', stdout=stdout)
                self.assertIsNone(value); self.assert_private_error(error)
                self.assertEqual(len(calls), 1)

    def test_ps_byte_cap_is_on_encoded_input_and_never_returns_a_partial_roster(self):
        for stdout in ('1 2\n'*(inventory.MAX_BYTES//4+1), '\N{LATIN SMALL LETTER E WITH ACUTE}'*inventory.MAX_BYTES):
            value, error, calls = self.call(platform='linux', stdout=stdout)
            self.assertIsNone(value); self.assertEqual(str(error), 'anchor-inventory-size')
            self.assertEqual(len(calls), 1)

    def test_injected_sleep_producer_timeout_reaps_only_its_exact_direct_child(self):
        # This portable subprocess.run contract check does not execute ps or
        # the native helper and does not claim all historical children are gone.
        original_run, original_popen = subprocess.run, subprocess.Popen
        children = []
        def record_child(*args, **kwargs):
            child = original_popen(*args, **kwargs); children.append(child); return child
        def sleep_producer(_args, **kwargs):
            self.assertEqual(kwargs['timeout'], 3.0)
            return original_run([sys.executable, '-c', 'import time;time.sleep(30)'], **kwargs)
        try:
            with mock.patch.object(inventory.sys, 'platform', 'darwin'), \
                 mock.patch.dict(inventory.os.environ, environment(), clear=True), \
                 mock.patch.object(inventory.subprocess, 'Popen', side_effect=record_child), \
                 mock.patch.object(inventory.subprocess, 'run', side_effect=sleep_producer):
                with self.assertRaises(inventory.InventoryError) as caught:
                    inventory.collect(GROUP)
            self.assertEqual(str(caught.exception), 'anchor-inventory-timeout')
            self.assertEqual(len(children), 1)
            self.assertIsNotNone(children[0].returncode)
            with self.assertRaises(ChildProcessError): os.waitpid(children[0].pid, os.WNOHANG)
            self.assertTrue(all(stream is None or stream.closed for stream in
                                (children[0].stdin, children[0].stdout, children[0].stderr)))
        finally:
            # subprocess.run owns timeout kill/reap. Independent bounded wait
            # must also hold on an assertion failure without a group signal.
            for child in children:
                if child.returncode is None: child.kill()
                child.wait(timeout=5)
                for stream in (child.stdin, child.stdout, child.stderr):
                    if stream is not None and not stream.closed: stream.close()


if __name__ == '__main__':
    unittest.main()
