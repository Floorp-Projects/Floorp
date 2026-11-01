# SPDX-License-Identifier: MPL-2.0
import hashlib
import copy
import json
import os
from pathlib import Path
import subprocess
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import nss_runtime as nss
from support import Failure
from test_mozilla_target_build import row
from test_nss_dynamic_closure import ClosureFixture


class RuntimeTests(ClosureFixture, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.calls = []
        self.runner = SimpleNamespace(records=[], failure=None, monitor_failure=None,
            limits={'jobMinutes': 360}, started=1, elapsed=lambda: 60)
        def run(stage, command, cwd, env, budget, **kwargs):
            self.calls.append((stage, command, cwd, env, budget, kwargs))
            self.runner.records.append(row(stage))
        self.runner.run = run
        self.result = {}

    def build(self, env=None):
        with mock.patch.object(nss, 'GRAPH_PINS', self.pins):
            nss.run_build(self.task, self.task, self.runner, env or {}, 345, self.result)

    def test_two_literal_targets_use_original_object_tree_clock_and_budget(self):
        self.build()
        self.assertEqual(self.result['completedStages'], [name for name, _ in nss.TARGETS])
        self.assertEqual(self.result['status'], 'built')
        self.assertFalse(self.result['graph']['graph']['shellReachesTargets']['softokn3'])
        for call, (_, node) in zip(self.calls, nss.TARGETS):
            self.assertEqual(call[1][1:], ['mach', 'build', '-j3', '-C', '.', node])
            self.assertEqual(call[2], self.task)
            self.assertEqual((call[4], call[5]['phase_deadline_minutes']), (360, 345))
        self.assertFalse(self.result['clockResetAllowed'])
        self.assertFalse(self.result['native42ResultQualification'])

    def test_hash_mismatch_stops_before_any_build(self):
        (self.task / 'obj/root.mk').write_bytes(self.root + b'# changed\n')
        with self.assertRaisesRegex(Failure, 'nss-runtime-graph-pin'):
            self.build()
        self.assertEqual(self.calls, [])

    def test_absent_or_opaque_declaration_is_rejected_even_with_matching_pin(self):
        path = self.task / 'obj/root.mk'
        for raw in (self.root.replace(nss.DYNAMIC_TARGETS['softokn3'].encode(), b''),
                    self.root + b'evil := $(shell exit 0)\n'):
            path.write_bytes(raw)
            self.pins['rootMk'] = self.pins['rootMk'][:2] + (hashlib.sha256(raw).hexdigest(),)
            with self.assertRaisesRegex(Failure, 'nss-runtime-node-missing'):
                self.build()
        self.assertEqual(self.calls, [])

    def test_graph_link_and_same_size_replacement_are_rejected(self):
        path = self.task / 'obj/root.mk'
        reference = path.with_name('reference')
        path.rename(reference)
        path.symlink_to(reference)
        with self.assertRaisesRegex(Failure, 'nss-runtime-graph-pin'):
            self.build()
        path.unlink()
        reference.rename(path)
        original = nss._FixedRead.verify
        def replace(reader):
            new = path.with_name('replacement')
            new.write_bytes(path.read_bytes())
            new.replace(path)
            original(reader)
        with mock.patch.object(nss._FixedRead, 'verify', replace), self.assertRaises(Exception):
            self.build()
        self.assertEqual(self.calls, [])

    def test_failed_first_stage_retains_its_failure_and_never_launches_second(self):
        def fail(*args, **kwargs):
            self.runner.failure = {'stage': nss.TARGETS[0][0], 'code': 'nonzero-exit'}
            raise Failure('nonzero-exit')
        self.runner.run = fail
        with self.assertRaisesRegex(Failure, 'nonzero-exit'):
            self.build()
        self.assertEqual(self.result['completedStages'], [])
        self.assertEqual(self.result['currentStage'], nss.TARGETS[0][0])
        self.assertEqual(self.runner.failure['code'], 'nonzero-exit')

    def test_missing_reap_or_changed_graph_stops_before_second_target(self):
        original = self.runner.run
        def incomplete(*args, **kwargs):
            original(*args, **kwargs)
            self.runner.records[-1]['primaryAnchor']['actualReaped'] = False
        self.runner.run = incomplete
        with self.assertRaisesRegex(Failure, 'nss-runtime-build-proof'):
            self.build()
        self.assertEqual(len(self.calls), 1)

    def test_graph_mutated_by_first_build_stops_before_second_target(self):
        original = self.runner.run
        def mutate(*args, **kwargs):
            original(*args, **kwargs)
            (self.task / 'obj/root.mk').write_bytes(self.root + b'# changed\n')
        self.runner.run = mutate
        with self.assertRaisesRegex(Failure, 'nss-runtime-graph-pin'):
            self.build()
        self.assertEqual(len(self.calls), 1)

    def test_deadline_cleanup_and_filter_environment_cannot_start_work(self):
        for env in ({'RECURSE_BASE_DIR': '1'}, {'FASTER_RECURSIVE_MAKE': '1'}):
            with self.assertRaisesRegex(Failure, 'nss-runtime-environment'):
                self.build(env)
        self.runner.records.append({'groupCleanup': 'unverified'})
        with self.assertRaisesRegex(Failure, 'nss-runtime-cleanup'):
            self.build()
        self.runner.records.clear()
        for elapsed in (345 * 60, float('nan'), -1):
            self.runner.elapsed = lambda: elapsed
            with self.assertRaisesRegex(Failure, 'nss-runtime-deadline'):
                self.build()
        self.assertEqual(self.calls, [])

    def prepare_tool(self):
        tools = self.task / 'tools'
        executable = tools / 'fix-stacks/fix-stacks'
        executable.parent.mkdir(parents=True)
        executable.write_text('#!/bin/sh\nexec /bin/cat\n')
        executable.chmod(0o700)
        self.env = {'PATH': '/usr/bin:/bin', 'MOZ_FETCHES_DIR': str(tools)}
        return executable

    def stack_runner(self, marker=True):
        def run(stage, command, cwd, env, budget, **kwargs):
            self.calls.append((stage, command, cwd, env, budget, kwargs))
            child = subprocess.run(command, cwd=cwd, env=env, capture_output=True, timeout=5)
            self.assertEqual(child.returncode, 0, child.stderr)
            (self.task / (stage + '.private.log')).write_bytes(child.stdout if marker else b'no marker\n')
            self.runner.records.append(row(stage))
        self.runner.run = run

    def test_actual_stock_wrapper_roundtrip_with_control_executable_is_bounded(self):
        executable = self.prepare_tool()
        self.stack_runner()
        nss.run_stack_probe(self.task, self.runner, self.env, 345, self.result)
        self.assertEqual(self.result['status'], 'passed')
        self.assertTrue(self.result['realToolExecuted'])
        self.assertEqual(self.result['tool']['sha256'], hashlib.sha256(executable.read_bytes()).hexdigest())
        self.assertEqual(self.calls[0][5]['phase_deadline_minutes'], 1.5)
        self.assertEqual(self.calls[0][5]['log_limit'], 1024**2)
        self.assertEqual(self.result['symbolicationQuality'], 'UNVERIFIED')
        self.assertNotIn(str(self.task), json.dumps(self.result))

    def test_missing_linked_tool_or_injected_environment_never_executes_wrapper(self):
        executable = self.prepare_tool()
        reference = executable.with_name('original')
        executable.rename(reference)
        for kind in ('missing', 'symlink', 'hardlink'):
            if executable.exists():
                executable.unlink()
            if kind == 'symlink':
                executable.symlink_to(reference)
            elif kind == 'hardlink':
                os.link(reference, executable)
            with self.assertRaisesRegex(Failure, 'nss-runtime-tool'):
                nss.run_stack_probe(self.task, self.runner, self.env, 345, {})
        with self.assertRaisesRegex(Failure, 'nss-runtime-environment'):
            nss.run_stack_probe(self.task, self.runner, {**self.env, 'DYLD_LIBRARY_PATH': '/other'}, 345, {})
        self.assertEqual(self.calls, [])

    def test_stack_success_without_exact_marker_is_not_accepted(self):
        self.prepare_tool()
        self.stack_runner(marker=False)
        with self.assertRaisesRegex(Failure, 'nss-runtime-stack-marker'):
            nss.run_stack_probe(self.task, self.runner, self.env, 345, self.result)
        self.assertFalse(self.result['realToolExecuted'])

    def prepare_load(self):
        self.config = self.task / 'effective-config.private.log'
        self.config.write_text(json.dumps({'substs': {'MOZ_MACBUNDLE_NAME': 'Test.app'}}))
        self.binary = self.task / 'obj/dist/Test.app/Contents/MacOS/xpcshell'
        self.binary.write_bytes(b'CONTROL-XPCSHELL')
        self.binary.chmod(0o700)
        self.binary.with_name('XUL').write_bytes(b'CONTROL-XUL')
        self.binary.with_name('XUL').chmod(0o700)
        self.macho = lambda path, kind: {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'kind': kind}
        self.assets = {'binaries': {key: self.macho(path, kind) for key, path, kind in
                                  [('xpcshell', self.binary, 2), ('XUL', self.binary.with_name('XUL'), 6)]}}

    def load(self, *, failed=False, after=lambda: None):
        def case(name, source, binary, manifest, test, task, parent, env, deadline, *, value):
            self.calls.append((name, binary, manifest, test, deadline))
            value.update(expectationSatisfied=not failed,
                         harness={'counts': {'subtestStatuses': {'PASS': 2}}})
            after()
        with mock.patch.object(nss.smoke, 'run_case', case):
            nss.run_load_probe(self.task, self.task, self.runner, {}, 345,
                               self.macho, self.assets, self.result)

    def test_same_built_shell_runs_separate_known_answer_without_qualifying_42(self):
        self.prepare_load()
        self.load()
        self.assertEqual(self.calls[0][:2], ('nss-load', self.binary))
        self.assertEqual(self.calls[0][-1], 3)
        self.assertEqual(self.result['status'], 'passed')
        self.assertTrue(self.result['realNSSLoadObserved'])
        self.assertFalse(self.result['native42ResultQualification'])
        self.assertEqual(self.result['exactDSOMapping'], 'UNVERIFIED')

    def test_failed_harness_and_same_byte_binary_replacement_reject_probe(self):
        self.prepare_load()
        with self.assertRaisesRegex(Failure, 'nss-runtime-load-proof'):
            self.load(failed=True)
        self.assertFalse(self.result['realNSSLoadObserved'])
        def replace():
            new = self.binary.with_name('replaced')
            new.write_bytes(self.binary.read_bytes())
            new.chmod(0o700)
            new.replace(self.binary)
        with self.assertRaisesRegex(Failure, 'nss-runtime-post-asset'):
            self.load(after=replace)
        self.assertFalse(self.result['realNSSLoadObserved'])

    def test_foreign_or_stale_binary_never_launches_probe(self):
        self.prepare_load()
        os.utime(self.binary, (0, 0))
        with self.assertRaisesRegex(Failure, 'nss-runtime-current-asset'):
            self.load()
        self.assertEqual(self.calls, [])

    def test_complete_control_proof_rejects_missing_stage_or_substituted_receipt(self):
        self.prepare_tool()
        self.stack_runner()
        stack = {}
        nss.run_stack_probe(self.task, self.runner, self.env, 345, stack)
        self.runner.run = lambda stage, *args, **kwargs: self.runner.records.append(row(stage))
        self.build()
        from test_fresh45 import target_fixture
        _, assets, smoke = target_fixture()
        case = copy.deepcopy(smoke['cases'][0])
        case.update(case='nss-load', expectedExitCode=0)
        case['harness']['counts'] = {'subtestStatuses': {'PASS': 2}}
        value = {'contract': nss.CONTRACT, 'productQualification': False,
            'native42ResultQualification': False, 'build': self.result, 'stackProbe': stack,
            'loadProbe': {'status': 'passed', 'realNSSLoadObserved': True, 'bindingsStable': True,
                'assets': assets['binaries'], 'fixtureSHA256': dict(nss.PROBE_INPUTS),
                'oracle': 'NSS-PK11-32bytes-and-Rust-SHA256-abc', 'case': case}}
        with mock.patch.object(nss, 'GRAPH_PINS', self.pins):
            self.assertTrue(nss.proof_complete(value, self.runner.records, assets))
            for change in (lambda d: d['stackProbe'].update(realToolExecuted=False),
                           lambda d: d['build']['graph']['graph']['targetsDeclared'].update(freebl3=False),
                           lambda d: d['loadProbe']['case'].update(case='compiled-debug'),
                           lambda d: d['loadProbe'].update(fixtureSHA256={}),
                           lambda d: d['loadProbe']['case']['harness']['counts']['subtestStatuses'].update(PASS=True)):
                bad = copy.deepcopy(value)
                change(bad)
                self.assertFalse(nss.proof_complete(bad, self.runner.records, assets))
            self.assertFalse(nss.proof_complete(value, self.runner.records[:-1], assets))


if __name__ == '__main__':
    unittest.main()
