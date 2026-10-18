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
import unittest
from unittest import mock
from types import SimpleNamespace

import checkpoint as cp
from support import Failure, Runner, digest


class CheckpointGuards(unittest.TestCase):
    def setUp(self):
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
        (self.task / 'checkpoint-boundary.json').write_text(json.dumps(
            {'status': 'passed', 'node': cp.NODE, 'newOwnedSurvivors': 0, 'interrupted': False}))
        (self.task / 'effective-config.private.log').write_text(json.dumps({'substs': {}}))
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
        for changes in ({'interrupted': True}, {'status': 'failed'}, {'newOwnedSurvivors': 1}):
            boundary = {'status': 'passed', 'node': cp.NODE, 'newOwnedSurvivors': 0, 'interrupted': False}
            (self.task / 'checkpoint-boundary.json').write_text(json.dumps({**boundary, **changes}))
            with self.subTest(changes=changes), self.assertRaisesRegex(Failure, 'checkpoint-boundary'):
                self.pack()
            self.assertFalse((self.task / 'checkpoint-ready.json').exists())

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
        before = cp.owned_processes()
        child = subprocess.Popen(['/bin/sleep', '10'], start_new_session=True)
        try:
            after = cp.owned_processes()
            self.assertTrue(any(row[0] == child.pid for row in after - before))
        finally:
            child.terminate()
            child.wait(timeout=5)
        self.assertFalse(any(row[0] == child.pid for row in cp.owned_processes() - before))

    def test_apple_signed_unrelated_uids_are_normalized_before_owner_filter(self):
        value = SimpleNamespace(pid=41, uid=501, ruid=501, start_sec=123, start_usec=4)
        probe = mock.Mock()
        probe.bsd.return_value = ('measured', value, None)
        inventory = SimpleNamespace(stdout='0 0 0\n41 501 501\n42 -2 -2\n43 4294967294 4294967294\n')
        with mock.patch.object(cp.subprocess, 'run', return_value=inventory), \
                mock.patch.object(cp.os, 'geteuid', return_value=501), \
                mock.patch.object(cp.os, 'getuid', return_value=501), \
                mock.patch('darwin_activity.probe', return_value=probe):
            self.assertEqual(cp.owned_processes(), {(41, 123, 4, 501, 501)})
        probe.bsd.assert_called_once_with(41)

    def test_malformed_or_invalid_uid_inventory_remains_unavailable(self):
        for uid in ('-1', '4294967295', '-2147483649', '4294967296', 'unknown', '+501'):
            with self.subTest(uid=uid), \
                    mock.patch.object(cp.subprocess, 'run', return_value=SimpleNamespace(stdout='42 '+uid+' 0\n')), \
                    mock.patch('darwin_activity.probe') as probe:
                with self.assertRaisesRegex(Failure, 'checkpoint-process-inventory-format'):
                    cp.owned_processes()
                probe.assert_not_called()

    def test_mixed_owner_identity_is_bound_to_inventory_and_retained_in_census(self):
        probe = mock.Mock()
        probe.bsd.return_value = ('measured', SimpleNamespace(pid=41, start_sec=123, start_usec=4,
                                                           uid=0, ruid=501), None)
        with mock.patch.object(cp.subprocess, 'run', return_value=SimpleNamespace(stdout='41 0 501\n')), \
                mock.patch.object(cp.os, 'geteuid', return_value=501), \
                mock.patch.object(cp.os, 'getuid', return_value=501), \
                mock.patch('darwin_activity.probe', return_value=probe):
            self.assertEqual(cp.owned_processes(), {(41,123,4,0,501)})
        probe.bsd.assert_called_once_with(41)

    def test_credentials_changed_since_inventory_remain_unavailable(self):
        probe = mock.Mock()
        probe.bsd.return_value = ('measured', SimpleNamespace(uid=501, ruid=501), None)
        with mock.patch.object(cp.subprocess, 'run', return_value=SimpleNamespace(stdout='41 0 501\n')), \
                mock.patch.object(cp.os, 'geteuid', return_value=501), \
                mock.patch.object(cp.os, 'getuid', return_value=501), \
                mock.patch('darwin_activity.probe', return_value=probe):
            with self.assertRaisesRegex(Failure, 'checkpoint-process-owner-changed'):
                cp.owned_processes()

    def test_same_birth_with_changed_credentials_is_not_in_baseline(self):
        probe = mock.Mock()
        def census(uid):
            probe.bsd.return_value = ('measured', SimpleNamespace(pid=41, start_sec=123, start_usec=4,
                                                               uid=uid, ruid=501), None)
            with mock.patch.object(cp.subprocess, 'run', return_value=SimpleNamespace(stdout=f'41 {uid} 501\n')), \
                    mock.patch.object(cp.os, 'geteuid', return_value=501), \
                    mock.patch.object(cp.os, 'getuid', return_value=501), \
                    mock.patch('darwin_activity.probe', return_value=probe):
                return cp.owned_processes()
        before = census(501)
        self.assertEqual(census(0)-before, {(41,123,4,0,501)})

    def test_unavailable_census_keeps_permission_and_transient_reasons_distinct(self):
        cases = [('permission',0,'checkpoint-process-permission-mixed'),
                 ('permission',501,'checkpoint-process-permission-worker'),
                 ('short-bsd-info',501,'checkpoint-process-short-bsd-info'),
                 ('invalid-identity',501,'checkpoint-process-invalid-identity'),
                 ('invalid-bsd-state',501,'checkpoint-process-invalid-bsd-state'),
                 ('unknown',501,'checkpoint-process-identity-unavailable')]
        for reason,uid,code in cases:
            probe = mock.Mock()
            probe.bsd.return_value = ('unavailable', None, reason)
            with self.subTest(reason=reason,uid=uid), \
                    mock.patch.object(cp.subprocess, 'run', return_value=SimpleNamespace(stdout=f'41 {uid} 501\n')), \
                    mock.patch.object(cp.os, 'geteuid', return_value=501), \
                    mock.patch.object(cp.os, 'getuid', return_value=501), \
                    mock.patch('darwin_activity.probe', return_value=probe):
                with self.assertRaisesRegex(Failure, code):
                    cp.owned_processes()


if __name__ == '__main__':
    unittest.main()
