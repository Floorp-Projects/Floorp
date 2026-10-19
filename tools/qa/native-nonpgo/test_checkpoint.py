# SPDX-License-Identifier: MPL-2.0
import io
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

import checkpoint as cp
from support import Failure, Runner, digest


def completed_records():
    return [{'stage': name, 'exitCode': 0, 'reason': None, 'failurePhase': None,
             'groupCleanup': 'verified-gone', 'leaderReap': {'status': 'reaped', 'exitCode': 0},
             'cleanupFailure': None} for name in ('configure', 'effective-config',
             'checkpoint-exports', 'checkpoint-rust', 'checkpoint-rust-archive')]


def cleanup_checkpoint_child(child):
    # addCleanup runs this independently of a test's primary exception. Reuse
    # the bounded exact-Popen TERM/KILL/reap path, never signal a census PID.
    from task_api_experiment import cleanup_child
    failure = None
    try:
        if child.stdin and not child.stdin.closed:
            child.stdin.close()
    except BaseException as error:
        failure = type(error).__name__
    receipt = {}
    cleanup_child(child, {}, None, receipt)
    if failure or receipt['cleanupFailures'] or not receipt['leaderReaped']:
        raise AssertionError('checkpoint-child-cleanup-incomplete')


class CompletedBuildTests(unittest.TestCase):
    def runner(self):
        return SimpleNamespace(failure=None, monitor_failure=None, groups=set(),
            records=completed_records(), observation_status='sampling',
            observed_processes={(90001,123456789,42)})

    def test_ambient_processes_are_not_build_survivors_or_resource_zero(self):
        runner = self.runner()
        details = {}
        def known(observed, receipt):
            self.assertEqual(observed, runner.observed_processes)
            receipt.update(candidateIdentities=239, selfBindingVerified=True,
                           trackedLiveBirths=0)
            return {'live': set(), 'zombies':set()}
        with mock.patch.object(cp, 'known_build_processes', side_effect=known):
            result = cp.completed_build(runner, details)
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['knownBuildIdentityCheck']['measurement']['candidateIdentities'],239)
        self.assertEqual(result['backgroundResourceUsage'], 'NOT_MEASURED_OUTSIDE_DECLARED_OWNED_SCOPE')
        self.assertNotIn('newOwnedSurvivors', result)
        self.assertNotIn('rssMiB', result)

    def test_known_live_birth_rejects_even_if_original_process_group_is_gone(self):
        runner = self.runner()
        details = {}
        with mock.patch.object(cp, 'known_build_processes', return_value={
                'live':runner.observed_processes,'zombies':set()}), \
                self.assertRaisesRegex(Failure, 'checkpoint-known-build-survivors'):
            cp.completed_build(runner, details)
        self.assertEqual(details['knownBuildIdentityCheck']['remainingLive'], 1)
        self.assertEqual(details['knownBuildIdentityCheck']['status'], 'rejected')

    def test_known_zombie_remains_a_cleanup_failure_even_though_not_a_writer(self):
        runner, details = self.runner(), {}
        with mock.patch.object(cp, 'known_build_processes', return_value={
                'live':set(),'zombies':runner.observed_processes}), \
                self.assertRaisesRegex(Failure, 'checkpoint-known-build-survivors'):
            cp.completed_build(runner, details)
        self.assertEqual(details['knownBuildIdentityCheck']['remainingLive'], 0)
        self.assertEqual(details['knownBuildIdentityCheck']['remainingZombies'], 1)

    def test_snapshot_unknown_and_coverage_loss_never_mean_no_survivors(self):
        runner = self.runner()
        details = {}
        with mock.patch.object(cp, 'known_build_processes', side_effect=Failure('census-sysctl-permission')), \
                self.assertRaisesRegex(Failure, 'census-sysctl-permission'):
            cp.completed_build(runner, details)
        self.assertIsNone(details['knownBuildIdentityCheck']['remainingLive'])
        for status in ('unavailable', 'truncated', 'disabled'):
            runner.observation_status = status
            with mock.patch.object(cp, 'known_build_processes') as snapshot, \
                    self.assertRaisesRegex(Failure, 'checkpoint-build-observation-unavailable'):
                cp.completed_build(runner, {})
            snapshot.assert_not_called()

    def test_failure_monitor_open_group_and_unreaped_or_stopped_stages_reject(self):
        for field, value in (('failure', {'code':'failed'}), ('monitor_failure', {'unknown':True}),
                             ('groups', {90001})):
            runner = self.runner()
            setattr(runner, field, value)
            with self.subTest(field=field), self.assertRaisesRegex(Failure, 'checkpoint-stage-completion'):
                cp.completed_build(runner, {})
        for changes in ({'exitCode':1}, {'reason':'controlled-stop'}, {'failurePhase':'cleanup'},
                        {'groupCleanup':'unverified'}, {'leaderReap':{'status':'still-running','exitCode':None}},
                        {'cleanupFailure':'denied'}):
            rows = completed_records()
            rows[-2].update(changes)
            with self.subTest(changes=changes), self.assertRaisesRegex(Failure, 'checkpoint-stage-completion'):
                cp.verify_completed_stages(rows)

    def test_cleanup_error_is_recorded_without_replacing_primary_test_failure(self):
        child = mock.Mock(stdin=None, returncode=None)
        child.wait.side_effect = subprocess.TimeoutExpired('controlled-child', 2)
        class ControlledFailure(unittest.TestCase):
            def runTest(self):
                self.addCleanup(cleanup_checkpoint_child, child)
                raise RuntimeError('controlled-primary-failure')
        result = unittest.TestResult()
        ControlledFailure().run(result)
        self.assertEqual(len(result.errors), 1)
        self.assertIn('controlled-primary-failure', result.errors[0][1])
        self.assertEqual(len(result.failures), 1)
        self.assertIn('checkpoint-child-cleanup-incomplete', result.failures[0][1])
        child.terminate.assert_called_once()
        child.kill.assert_called_once()
        for rows in (completed_records()[:-1], completed_records()+[completed_records()[-1]]):
            with self.assertRaisesRegex(Failure, 'checkpoint-required-stage'):
                cp.verify_completed_stages(rows)


class CheckpointGuards(unittest.TestCase):
    def setUp(self):
        from process_census import BINDING_KEYS
        self.real_census_binding = {key:os.environ[key] for key in BINDING_KEYS if key in os.environ}
        self.temporary = tempfile.TemporaryDirectory(prefix='floorp-checkpoint-contract-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.task, self.source = self.root / 'task', self.root / 'source'
        self.task.mkdir(mode=0o700)
        self.source.mkdir()
        self.sdk = self.task / 'sdk'
        for path in (self.sdk, self.task / 'tools', self.task / 'obj', self.task / 'mozbuild'):
            path.mkdir()
        (self.source / 'host.h').write_text('public source fixture\n')
        (self.sdk / 'header.h').write_text('SDK fixture\n')
        (self.task / 'tools/compiler').write_bytes(b'compiler fixture')
        (self.task / 'obj/.fingerprint').mkdir()
        (self.task / 'obj/.fingerprint/completed').write_text('completed Rust fixture\n')
        (self.task / 'obj/object.o').write_bytes(b'completed object fixture')
        (self.task / 'obj/object.o').chmod(0o751)
        (self.task / 'obj/source-link').symlink_to(self.source / 'host.h')
        (self.task / 'obj/inside-link').symlink_to('object.o')
        identity = self.source.name + '-' + hashlib.sha256(str(self.source).encode()).hexdigest()[:12]
        self.venv = self.task / 'mozbuild/srcdirs' / identity / '_virtualenvs/build'
        self.venv.mkdir(parents=True)
        (self.venv / 'package.py').write_text('verified package fixture\n')
        (self.task / 'started-monotonic').write_text(str(time.monotonic()))
        (self.task / 'started').write_text(str(time.time()))
        (self.task / 'first-failure.json').write_text('{"code":"OLD-QA-STATE"}\n')
        self.rust_archive = self.task / 'obj/x86_64-apple-darwin/release/libgkrust.a'
        self.rust_archive.parent.mkdir(parents=True)
        self.rust_archive.write_bytes(b'!<arch>\nfixture member\n')
        self.rust_archive.with_suffix('.d').write_text('libgkrust.a: fixture.rs\n')
        self.boundary = {'status': 'passed', 'contract': cp.BOUNDARY_CONTRACT, 'node': cp.NODE,
            'knownBuildSurvivors': 0, 'interrupted': False, 'stages': completed_records(),
            'knownBuildIdentityCheck': {'status':'passed','remainingLive':0,'remainingZombies':0,'observedBirths':3},
            'archiveSHA256':digest(self.rust_archive),
            'depInfoSHA256':digest(self.rust_archive.with_suffix('.d'))}
        (self.task / 'checkpoint-boundary.json').write_text(json.dumps(self.boundary))
        (self.task / 'effective-config.private.log').write_text(json.dumps({'substs': {
            'RUST_TARGET':'x86_64-apple-darwin','MOZ_CARGO_DEFAULT_PROFILE_DIR':'release',
            'MOZ_DEBUG_RUST':False}}))
        self.plan = json.loads(Path(__file__).with_name('plan.json').read_text())
        self.plan['checkpoint'].update(maxStoredBytes=1024**2, maxLogicalBytes=1024**2,
                                       maxEntries=100, maxReferenceEntries=100)
        self.plan['limits']['minFreeDuringBuildGiB'] = 0
        self.plan_path = self.root / 'plan.json'
        self.save_plan()
        self.env = mock.patch.dict(os.environ, {'FLOORP_FIXTURE_SHA': 'a' * 40, 'HARNESS_SHA': 'b' * 40,
            'RUNTIME_SHA': 'c' * 40, 'GITHUB_REPOSITORY': 'Floorp-Projects/Floorp-Runtime',
            'GITHUB_RUN_ID': '1234', 'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_SHA': 'd' * 40,
            'ImageVersion': 'fixture-image', 'MOZ_BUILD_DATE': '20261018120000'})
        self.env.start()
        self.addCleanup(self.env.stop)

    def save_plan(self):
        self.plan_path.write_text(json.dumps(self.plan))

    def test_boundary_rejection_retains_full_counts_and_bounded_safe_deltas(self):
        before = {(90001, 123456789, 654321, 501, 501), (90002, 123456789, 654321, 0, 501)}
        after = {(90001, 123456790, 654321, 501, 501), (90002, 123456789, 654321, 501, 501)}
        after.update((pid, 123456791, 654321, 501, 501) for pid in range(90003, 90023))
        receipt = {}
        def snapshot(details):
            details.update(candidateIdentities=len(after), selfBindingVerified=True)
            return after
        with mock.patch.object(cp, 'owned_processes', side_effect=snapshot), \
                self.assertRaisesRegex(Failure, 'checkpoint-owned-survivors'):
            cp.verify_owned_boundary(before, receipt)
        self.assertEqual(receipt['status'], 'rejected')
        self.assertEqual(receipt['candidateIdentities'], 22)
        self.assertEqual(receipt['newCandidateIdentities'], 22)
        self.assertEqual(receipt['removedCandidateIdentities'], 2)
        self.assertEqual(receipt['deltaKinds'], {'new-pid': 20, 'pid-generation-changed': 1,
                                                'candidate-credentials-changed': 1})
        self.assertEqual(len(receipt['deltaRecords']), 16)
        self.assertTrue(receipt['deltaRecordsTruncated'])
        self.assertFalse(receipt['signalOwnershipGranted'])
        self.assertTrue(all(set(row)=={'kind','identityTag'} for row in receipt['deltaRecords']))
        for row in receipt['deltaRecords']:
            self.assertRegex(row['identityTag'], r'^[0-9a-f]{16}$')
        self.assertTrue(set(receipt).isdisjoint({'pid', 'uid', 'ruid', 'startSec', 'startUsec',
                                                'identities', 'tagKey'}))

    def test_boundary_unavailable_keeps_unknown_and_original_failure(self):
        receipt = {}
        with mock.patch.object(cp, 'owned_processes', side_effect=Failure('census-sysctl-permission')), \
                self.assertRaisesRegex(Failure, 'census-sysctl-permission'):
            cp.verify_owned_boundary(set(), receipt)
        self.assertEqual(receipt['status'], 'unavailable')
        self.assertIsNone(receipt['newCandidateIdentities'])
        self.assertIsNone(receipt['deltaRecords'])

    def test_optional_delta_diagnostic_failure_cannot_replace_boundary_rejection(self):
        receipt = {}
        with mock.patch.object(cp, 'owned_processes', return_value={(101, 123, 4, 501, 501)}), \
                mock.patch.object(cp.os, 'urandom', side_effect=OSError('private diagnostic text')), \
                self.assertRaisesRegex(Failure, 'checkpoint-owned-survivors'):
            cp.verify_owned_boundary(set(), receipt)
        self.assertEqual(receipt['status'], 'rejected')
        self.assertEqual(receipt['newCandidateIdentities'], 1)
        self.assertEqual(receipt['deltaKinds'], {'new-pid': 1})
        self.assertEqual(receipt['deltaDiagnosticStatus'], 'unavailable')
        self.assertIsNone(receipt['deltaRecords'])
        self.assertIsNone(receipt['deltaRecordsTruncated'])
        self.assertNotIn('private diagnostic text', json.dumps(receipt))

    def test_optional_process_hints_never_exclude_candidates(self):
        for hint in ({'status':'unavailable'}, {'status':'identity-verified','role':'preferences-service',
                                              'observedInStageGroup':'observed'}):
            receipt = {}
            with mock.patch.object(cp, 'owned_processes', return_value={(101,123,4,501,501)}), \
                    mock.patch('census_diagnostics.describe', return_value=hint), \
                    self.assertRaisesRegex(Failure, 'checkpoint-owned-survivors'):
                cp.verify_owned_boundary(set(), receipt, set())
            self.assertEqual(receipt['newCandidateIdentities'],1)
            self.assertEqual(receipt['deltaRecords'][0]['processHint'],hint)

    def test_boundary_identical_or_removed_candidates_pass_and_tags_are_unlinkable(self):
        baseline = {(101, 123, 4, 501, 501), (102, 124, 5, 501, 501)}
        receipt = {}
        with mock.patch.object(cp, 'owned_processes', return_value={next(iter(baseline))}), \
                mock.patch.object(cp.os, 'urandom', side_effect=AssertionError('unnecessary random call')):
            self.assertIs(cp.verify_owned_boundary(baseline, receipt), receipt)
        self.assertEqual(receipt['status'], 'passed')
        self.assertEqual(receipt['newCandidateIdentities'], 0)
        self.assertEqual(receipt['removedCandidateIdentities'], 1)
        self.assertEqual(receipt['deltaRecords'], [])
        records = []
        for _ in range(2):
            receipt = {}
            with mock.patch.object(cp, 'owned_processes', return_value={(103, 125, 6, 501, 501)}), \
                    self.assertRaisesRegex(Failure, 'checkpoint-owned-survivors'):
                cp.verify_owned_boundary(baseline, receipt)
            records.append(receipt['deltaRecords'])
        self.assertNotEqual(records[0][0]['identityTag'], records[1][0]['identityTag'])

    def pack(self):
        return cp.pack(self.plan_path, self.source, self.task, self.sdk, time.monotonic() + 20)

    def consumer(self, ready):
        (self.task / 'checkpoint-download.json').write_text(json.dumps(ready))
        for name in cp.STATES:
            path = self.task / name
            if path.exists():
                shutil.rmtree(path)
        (self.task / 'first-failure.json').unlink()
        (self.task / 'effective-config.private.log').unlink()
        self.new_origin = str(time.monotonic())
        (self.task / 'started-monotonic').write_text(self.new_origin)

    def restore(self):
        return cp.restore(self.plan_path, self.source, self.task, self.sdk, time.monotonic() + 20)

    def test_roundtrip_preserves_hidden_state_mode_mtime_links_and_new_qa_origin(self):
        original = (self.task / 'obj/object.o').stat()
        ready = self.pack()
        self.consumer(ready)
        os.utime(self.source / 'host.h', None)
        restored = self.restore()
        self.assertEqual(restored['qualification'], self.plan['checkpoint']['qualification'])
        self.assertFalse(restored['freshObjdir'])
        self.assertFalse(restored['sameJob'])
        self.assertEqual((self.task / 'obj/object.o').stat().st_mtime_ns, original.st_mtime_ns)
        self.assertEqual((self.task / 'obj/object.o').stat().st_mode & 0o777, 0o751)
        self.assertEqual((self.task / 'obj/.fingerprint/completed').read_text(), 'completed Rust fixture\n')
        self.assertEqual((self.task / 'obj/source-link').resolve(), self.source / 'host.h')
        self.assertEqual((self.task / 'obj/inside-link').read_bytes(), b'completed object fixture')
        self.assertEqual((self.task / 'started-monotonic').read_text(), self.new_origin)
        self.assertFalse((self.task / 'first-failure.json').exists())
        with tarfile.open(self.task / 'checkpoint.tar.gz') as archive:
            self.assertFalse(any('first-failure' in row.name or 'started-monotonic' in row.name for row in archive))
        self.assertFalse((self.task / 'effective-config.private.log').exists())
        self.assertTrue((self.task / 'checkpoint-expected-config.json').exists())

    def test_completed_mapping_writer_can_be_packed_and_restored_after_exact_reap(self):
        target = self.task / 'obj/object.o'
        child = subprocess.Popen([sys.executable, '-c',
            'import mmap,os,sys; f=open(sys.argv[1],"r+b"); '
            'm=mmap.mmap(f.fileno(),0); f.close(); '
            'm[:]=b"x"*len(m); m.flush(); m.close()', str(target)])
        self.addCleanup(cleanup_checkpoint_child, child)
        self.assertEqual(child.wait(timeout=5), 0)
        expected = b'x' * len(b'completed object fixture')
        self.assertEqual(target.read_bytes(), expected)
        ready = self.pack()
        self.consumer(ready)
        self.restore()
        self.assertEqual(target.read_bytes(), expected)

    def test_late_object_after_manifest_enumeration_prevents_ready(self):
        original = cp.tree
        changed = False
        def enumerate_then_add(*args, **kwargs):
            nonlocal changed
            rows = original(*args, **kwargs)
            if kwargs.get('state_name') == 'mozbuild' and not changed:
                changed = True
                (self.task / 'obj/late-object.o').write_bytes(b'late writer fixture')
            return rows
        with mock.patch.object(cp, 'tree', side_effect=enumerate_then_add), \
                self.assertRaisesRegex(Failure, 'checkpoint-state-changed-after-pack'):
            self.pack()
        self.assertTrue(changed)
        self.assertFalse((self.task / 'checkpoint-ready.json').exists())

    def test_reference_or_new_state_created_during_readback_prevents_ready(self):
        original = cp.inspect
        for kind in ('reference', 'new-state'):
            with self.subTest(kind=kind):
                def inspect_then_change(*args, **kwargs):
                    result = original(*args, **kwargs)
                    if kind == 'reference':
                        (self.source / 'late-header.h').write_bytes(b'late reference fixture')
                    else:
                        (self.task / 'cargo').mkdir()
                    return result
                code = ('checkpoint-reference-changed-after-pack' if kind == 'reference'
                        else 'checkpoint-state-root-changed-after-pack')
                with mock.patch.object(cp, 'inspect', side_effect=inspect_then_change), \
                        self.assertRaisesRegex(Failure, code):
                    self.pack()
                self.assertFalse((self.task / 'checkpoint-ready.json').exists())
                (self.task / 'checkpoint.tar.gz').unlink()
                if kind == 'reference':
                    (self.source / 'late-header.h').unlink()
                else:
                    (self.task / 'cargo').rmdir()

    def test_mutation_during_exact_tar_stream_read_prevents_ready(self):
        original = cp.PackedStream.read
        changed = False
        def read_then_mutate(stream, size=-1):
            nonlocal changed
            data = original(stream, size)
            if not changed and data == b'completed object fixture':
                changed = True
                (self.task / 'obj/object.o').write_bytes(b'changed while streaming')
            return data
        with mock.patch.object(cp.PackedStream, 'read', read_then_mutate), \
                self.assertRaisesRegex(Failure, 'checkpoint-file-changed'):
            self.pack()
        self.assertTrue(changed)
        self.assertFalse((self.task / 'checkpoint-ready.json').exists())

    def test_mach_logs_are_omitted_but_required_cargo_metadata_is_preserved(self):
        logs = self.task / 'mozbuild/logs/build'
        logs.mkdir(parents=True)
        (logs / 'private.log').write_text('PRIVATE-MACH-LOG-SENTINEL')
        private = self.task / 'obj/.mozbuild'
        private.mkdir()
        (private / 'last_log.json').write_text('PRIVATE-MACH-LOG-SENTINEL')
        cargo = self.task / 'obj/x86_64-apple-darwin/release/build/unit/output'
        cargo.parent.mkdir(parents=True)
        cargo.write_text('cargo:rustc-link-lib=fixture\n')
        self.pack()
        with tarfile.open(self.task / 'checkpoint.tar.gz') as archive:
            entries = {item.name: archive.extractfile(item).read() for item in archive}
        self.assertFalse(any('PRIVATE-MACH-LOG-SENTINEL' in data.decode(errors='ignore') for data in entries.values()))
        self.assertEqual(entries['payload/obj/x86_64-apple-darwin/release/build/unit/output'],
                         b'cargo:rustc-link-lib=fixture\n')

    def test_actual_consumer_config_must_match_restored_contract(self):
        ready = self.pack()
        self.consumer(ready)
        self.restore()
        expected = json.loads((self.task / 'checkpoint-expected-config.json').read_text())
        (self.task / 'effective-config.private.log').write_text(json.dumps(expected))
        cp.verify_restored_config(self.task)
        expected['substs']['MOZ_DEBUG'] = 'changed'
        (self.task / 'effective-config.private.log').write_text(json.dumps(expected))
        with self.assertRaisesRegex(Failure, 'checkpoint-actual-config-mismatch'):
            cp.verify_restored_config(self.task)

    def test_full_log_receipt_retains_early_and_interleaved_error_classes(self):
        path = self.task / 'compile.private.log'
        path.write_text("x.cc:1: error: use of undeclared identifier 'PRIVATE-SENTINEL'\n" +
                        'ordinary progress\n' * 20000 + 'make: Error 2 KeyboardInterrupt\n')
        value = cp.full_log_receipt(path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['bytes'], path.stat().st_size)
        self.assertEqual(value['diagnosticCounts']['source-error'], 1)
        self.assertEqual(value['diagnosticCounts']['unclassified-error-marker'], 1)
        self.assertEqual(value['records'][0]['line'], 1)
        self.assertNotIn('PRIVATE-SENTINEL', json.dumps(value))

    def test_incomplete_or_symlink_log_is_unknown(self):
        path = self.task / 'compile.private.log'
        path.write_bytes(b'unterminated diagnostic')
        self.assertFalse(cp.full_log_receipt(path)['complete'])
        path.unlink()
        path.symlink_to(self.source / 'host.h')
        self.assertFalse(cp.full_log_receipt(path)['complete'])

    def test_interrupted_or_residual_process_boundary_never_packs(self):
        for changes in ({'interrupted': True}, {'status': 'failed'}, {'knownBuildSurvivors': 1},
                        {'contract':'legacy-global-uid-zero'}):
            boundary = self.boundary
            (self.task / 'checkpoint-boundary.json').write_text(json.dumps({**boundary, **changes}))
            with self.subTest(changes=changes), self.assertRaisesRegex(Failure, 'checkpoint-boundary'):
                self.pack()
            self.assertFalse((self.task / 'checkpoint-ready.json').exists())

    def test_boundary_archive_and_depinfo_changes_prevent_ready(self):
        for path in (self.rust_archive, self.rust_archive.with_suffix('.d')):
            before = path.read_bytes()
            path.write_bytes(before + b'late changed output\n')
            with self.subTest(output=path.suffix), self.assertRaisesRegex(Failure, 'checkpoint-boundary-output-changed'):
                self.pack()
            self.assertFalse((self.task / 'checkpoint-ready.json').exists())
            path.write_bytes(before)

    def test_caps_stop_before_ready_or_upload(self):
        self.plan['checkpoint']['maxStoredBytes'] = 100
        self.save_plan()
        with self.assertRaisesRegex(Failure, 'checkpoint-stored-cap'):
            self.pack()
        self.assertLessEqual((self.task / 'checkpoint.tar.gz').stat().st_size, 100)
        self.assertFalse((self.task / 'checkpoint-ready.json').exists())

    def test_logical_cap_prevents_archive(self):
        self.plan['checkpoint']['maxLogicalBytes'] = 1
        self.save_plan()
        with self.assertRaisesRegex(Failure, 'checkpoint-logical-cap'):
            self.pack()
        self.assertFalse((self.task / 'checkpoint.tar.gz').exists())

    def test_unbound_state_symlink_and_special_file_are_rejected(self):
        link = self.task / 'obj/escape'
        link.symlink_to('/outside-allowed-roots/fixture')
        with self.assertRaisesRegex(RuntimeError, 'checkpoint-external-link'):
            self.pack()
        link.unlink()
        os.mkfifo(link)
        with self.assertRaisesRegex(Failure, 'checkpoint-special-file'):
            self.pack()

    def test_changed_reference_content_is_rejected_before_state_restore(self):
        ready = self.pack()
        self.consumer(ready)
        (self.source / 'host.h').write_text('changed source\n')
        with self.assertRaisesRegex(Failure, 'checkpoint-reference-content'):
            self.restore()
        self.assertFalse((self.task / 'obj').exists())

    def test_reference_mtime_restore_must_preserve_exact_nanoseconds(self):
        ready = self.pack()
        self.consumer(ready)
        path = self.source / 'host.h'
        os.utime(path, ns=(path.stat().st_mtime_ns + 1000000000,) * 2)
        real_utime = os.utime
        def omit_reference_restore(target, *args, **kwargs):
            if Path(target) != path:
                real_utime(target, *args, **kwargs)
        with mock.patch.object(cp.os, 'utime', side_effect=omit_reference_restore):
            with self.assertRaisesRegex(Failure, 'checkpoint-mtime-precision'):
                self.restore()

    def test_changed_context_or_build_date_is_rejected_before_state_restore(self):
        ready = self.pack()
        self.consumer(ready)
        with mock.patch.dict(os.environ, {'GITHUB_SHA': 'e' * 40}):
            with self.assertRaisesRegex(Failure, 'checkpoint-context-mismatch'):
                self.restore()
        with mock.patch.dict(os.environ, {'MOZ_BUILD_DATE': '20261018130000'}):
            with self.assertRaisesRegex(Failure, 'checkpoint-producer-completion'):
                self.restore()
        self.assertFalse((self.task / 'obj').exists())

    def test_archive_digest_failure_prevents_extract(self):
        ready = self.pack()
        self.consumer(ready)
        with (self.task / 'checkpoint.tar.gz').open('ab') as output:
            output.write(b'TAMPER')
        with self.assertRaisesRegex(Failure, 'checkpoint-download-integrity'):
            self.restore()
        self.assertFalse((self.task / 'checkpoint-restore').exists())

    def test_payload_traversal_and_duplicate_are_rejected(self):
        ready = self.pack()
        path = self.task / 'checkpoint.tar.gz'
        with tarfile.open(path) as archive:
            entries = [(item.name, archive.extractfile(item).read()) for item in archive]
        for extra in ('../escape', entries[1][0]):
            with self.subTest(extra=extra):
                trial = self.root / 'trial.tar.gz'
                with tarfile.open(trial, 'w:gz') as archive:
                    for name, data in entries + [(extra, b'attack')]:
                        member = tarfile.TarInfo(name)
                        member.size = len(data)
                        archive.addfile(member, io.BytesIO(data))
                with self.assertRaisesRegex(Failure, 'checkpoint-payload-member'):
                    cp.inspect(trial, self.plan['checkpoint'], time.monotonic() + 10, ready['manifestSHA256'])

    def test_existing_state_is_not_overwritten(self):
        ready = self.pack()
        self.consumer(ready)
        (self.task / 'obj').mkdir()
        (self.task / 'obj/preserve').write_text('preserve')
        with self.assertRaisesRegex(Failure, 'checkpoint-state-not-empty'):
            self.restore()
        self.assertEqual((self.task / 'obj/preserve').read_text(), 'preserve')

    def test_phase_deadline_blocks_launch(self):
        runner = Runner(self.task, time.time(), self.plan['limits'])
        runner.monotonic_started = time.monotonic() - 61
        with mock.patch('support.subprocess.Popen') as launch:
            with self.assertRaisesRegex(Failure, 'phase-deadline-before-launch'):
                runner.run('checkpoint-rust', [sys.executable, '-c', 'pass'], self.task, {}, 360,
                           phase_deadline_minutes=1)
        launch.assert_not_called()

    def graph_fixture(self):
        obj = self.task / 'obj'
        (obj / 'root.mk').write_text('compile_targets := ' + cp.NODE + '\n' +
                                     'rust_targets := ' + cp.NODE + '\ninclude root-deps.mk\n')
        (obj / 'root-deps.mk').write_text('recurse_compile: ' + cp.NODE + '\nrecurse_rust: ' + cp.NODE + '\n')
        backend = obj / 'toolkit/library/rust'
        backend.mkdir(parents=True)
        (backend / 'backend.mk').write_text(
            'RUST_LIBRARY_FILE := $(DEPTH)/x86_64-apple-darwin/release/libgkrust.a\n'
            'CARGO_TARGET_DIR := $(DEPTH)\nCARGO_FILE := $(srcdir)/Cargo.toml\n'
            'RUST_LIBRARY_FEATURES := fixture-feature\n')
        (self.source / 'toolkit/library/rust').mkdir(parents=True)
        (self.source / 'toolkit/library/rust/Cargo.toml').write_text('[package]\nname="fixture"\n')
        (obj / '.cargo').mkdir()
        (obj / '.cargo/config.toml').write_text('[source.crates-io]\n')
        return {'substs': {'RUST_TARGET': 'x86_64-apple-darwin', 'MOZ_DEBUG_RUST': False,
                           'MOZ_CARGO_DEFAULT_PROFILE_DIR': 'release'}}

    def test_root_graph_selects_rust_profile_from_effective_config(self):
        data = self.graph_fixture()
        boundary = cp.verify_graph(self.task, self.source, data)
        self.assertEqual(boundary['node'], cp.NODE)
        self.assertEqual(boundary['profile'], 'release')
        self.assertTrue(boundary['archive'].endswith('/release/libgkrust.a'))

    def test_missing_or_duplicate_graph_node_is_rejected(self):
        data = self.graph_fixture()
        path = self.task / 'obj/root.mk'
        original = path.read_text()
        for content in (original.replace('compile_targets := ' + cp.NODE, 'compile_targets := other'),
                        original + 'compile_targets := ' + cp.NODE + '\n'):
            path.write_text(content)
            with self.subTest(content=content), self.assertRaises(Failure):
                cp.verify_graph(self.task, self.source, data)

    def test_backend_path_outside_object_root_is_rejected(self):
        data = self.graph_fixture()
        path = self.task / 'obj/toolkit/library/rust/backend.mk'
        path.write_text(path.read_text().replace('CARGO_TARGET_DIR := $(DEPTH)', 'CARGO_TARGET_DIR := /outside'))
        with self.assertRaisesRegex(Failure, 'checkpoint-cargo-target-dir'):
            cp.verify_graph(self.task, self.source, data)

    @unittest.skipUnless(sys.platform == 'darwin', 'actual Darwin ownership census gate')
    def test_real_mac_new_owned_process_is_observed(self):
        with mock.patch.dict(os.environ,self.real_census_binding):
            before = cp.owned_processes()
            child = subprocess.Popen(['/bin/sleep', '10'], start_new_session=True)
            try:
                after = cp.owned_processes()
                self.assertTrue(any(row[0] == child.pid for row in after - before))
            finally:
                child.terminate()
                child.wait(timeout=5)
            self.assertFalse(any(row[0] == child.pid for row in cp.owned_processes() - before))

    @unittest.skipUnless(sys.platform == 'darwin', 'actual Darwin known-child lifecycle')
    def test_real_mac_known_child_live_zombie_and_exact_reap(self):
        from darwin_activity import probe
        with mock.patch.dict(os.environ, self.real_census_binding):
            child = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.read()'],
                                     stdin=subprocess.PIPE)
            self.addCleanup(cleanup_checkpoint_child, child)
            state, identity, _ = probe().bsd(child.pid)
            self.assertEqual(state, 'measured')
            birth = (identity.pid,identity.start_sec,identity.start_usec)
            value = cp.known_build_processes({birth}, {})
            self.assertEqual(value, {'live':{birth},'zombies':set()})
            child.stdin.close()
            deadline = time.monotonic() + 5
            while True:
                value = cp.known_build_processes({birth}, {})
                if value['zombies']:
                    break
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.01)
            self.assertEqual(value, {'live':set(),'zombies':{birth}})
            self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual(cp.known_build_processes({birth}, {}), {'live':set(),'zombies':set()})


if __name__ == '__main__':
    unittest.main()
