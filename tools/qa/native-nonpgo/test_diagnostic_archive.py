# SPDX-License-Identifier: MPL-2.0
"""Exercise actual authenticated encryption using disposable keys inside the sandbox."""
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

import diagnostic_archive as archive
from firefox_target_observer import Unknown
from qa_core import Failure


class DiagnosticArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.keys.cleanup)
        cls.certificate = Path(cls.keys.name)/'recipient.pem'
        cls.key = Path(cls.keys.name)/'recipient.key'
        cls.openssl = ('/opt/homebrew/opt/openssl@3/bin/openssl' if platform.machine() == 'arm64'
                       else '/usr/local/opt/openssl@3/bin/openssl') if sys.platform == 'darwin' else '/usr/bin/openssl'
        subprocess.run([cls.openssl, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
            '-subj', '/CN=Disposable QA regression key', '-keyout', str(cls.key),
            '-out', str(cls.certificate)], check=True, capture_output=True, timeout=30,
            env={'PATH': '/usr/bin:/bin', 'OPENSSL_CONF': '/dev/null'})
        cls.certificate_hash = hashlib.sha256(cls.certificate.read_bytes()).hexdigest()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.task = self.root/'task'
        self.task.mkdir(mode=0o700)
        self.output = self.root/'encrypted.cms'
        self.environment = {'GITHUB_REPOSITORY': archive.REPOSITORY,
            'GITHUB_REF': 'refs/heads/'+archive.DIAGNOSTICS_BRANCH, 'GITHUB_EVENT_NAME': 'push',
            'GITHUB_RUN_ATTEMPT': '1', 'GITHUB_RUN_ID': '123', 'GITHUB_SHA': 'a'*40,
            'FLOORP_FIXTURE_SHA': 'b'*40, 'RUNTIME_SHA': 'c'*40, 'SECRET': 'ENVIRONMENT-SENTINEL'}
        self.origin = {**archive.bindings(self.environment), 'profile': archive.DIAGNOSTICS_PROFILE,
            'job': 456, 'jobName': 'fresh-firefox45', 'platformMinutes': 360,
            'workDeadlineMinutes': 345, 'clockResetAllowed': False}
        self.write('job-origin.json', json.dumps(self.origin).encode())
        self.raw = b'{"action":"log","message":"PRIVATE-RAW-SENTINEL"}\n\xffpartial'
        self.write(archive.SCOPED[0], self.raw)
        self.write(archive.SCOPED[1], b'Traceback PRIVATE-CONSOLE-SENTINEL\n')

    def write(self, name, raw):
        path = self.task/name
        path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        path.write_bytes(raw)
        path.chmod(0o600)

    def export(self, **overrides):
        arguments = {'task': self.task, 'certificate': self.certificate,
            'certificate_sha256': self.certificate_hash, 'output': self.output,
            'openssl': self.openssl, 'environment': self.environment}
        return archive.export(**{**arguments, **overrides})

    def decrypt(self, ciphertext=None):
        return subprocess.run([self.openssl, 'cms', '-decrypt', '-binary', '-inform', 'DER',
            '-recip', str(self.certificate), '-inkey', str(self.key)],
            input=self.output.read_bytes() if ciphertext is None else ciphertext,
            capture_output=True, timeout=30, env={'PATH': '/usr/bin:/bin', 'OPENSSL_CONF': '/dev/null'})

    def members(self):
        result = self.decrypt()
        self.assertEqual(result.returncode, 0)
        with tarfile.open(fileobj=io.BytesIO(result.stdout), mode='r:') as bundle:
            self.assertTrue(all(row.isfile() and row.mode == 0o600 and row.uid == 0 for row in bundle))
            return {row.name: bundle.extractfile(row).read() for row in bundle}

    def test_complete_ciphertext_roundtrip_retains_exact_even_nonterminal_binary_log(self):
        receipt = self.export()
        ciphertext = self.output.read_bytes()
        self.assertNotIn(b'PRIVATE-RAW-SENTINEL', ciphertext)
        self.assertNotIn('PRIVATE-', json.dumps(receipt))
        self.assertEqual(receipt['ciphertextSHA256'], hashlib.sha256(ciphertext).hexdigest())
        self.assertEqual(self.output.stat().st_mode & 0o777, 0o600)
        members = self.members()
        self.assertEqual(members[archive.SCOPED[0]], self.raw)
        manifest = json.loads(members['manifest.json'])
        self.assertEqual(manifest['binding'], archive.bindings(self.environment))
        self.assertIs(receipt['scopedFilesCaptured'], True)
        self.assertIs(receipt['productQualification'], False)
        self.assertIsNone(receipt['qualification'])
        self.assertNotIn('completeLog', receipt)

    def test_sdk_arbitrary_files_keys_and_environment_are_not_collected(self):
        for name in ('sdk-download/macos_sdk.tar.gz', 'obj/SDK/file', 'private.key', '.credentials'):
            self.write(name, b'SDK-AND-KEY-SENTINEL')
        self.export()
        members = self.members()
        self.assertLessEqual(set(members), {'manifest.json', *(name for name, _ in archive.FILES)})
        joined = b''.join(members.values())
        self.assertNotIn(b'SDK-AND-KEY-SENTINEL', joined)
        self.assertNotIn(b'ENVIRONMENT-SENTINEL', joined)
        self.assertNotIn(b'PRIVATE KEY', joined)

    def test_modified_gcm_tag_is_rejected_before_recovery_can_use_plaintext(self):
        self.export()
        raw = bytearray(self.output.read_bytes())
        raw[-1] ^= 1
        self.assertNotEqual(self.decrypt(bytes(raw)).returncode, 0)

    def test_swapped_certificate_pin_or_invalid_certificate_leaves_no_export(self):
        with self.assertRaisesRegex(Failure, 'diagnostic-recipient'):
            self.export(certificate_sha256='0'*64)
        broken = self.root/'broken.pem'
        broken.write_bytes(b'PRIVATE-CERTIFICATE-ERROR-SENTINEL')
        with self.assertRaisesRegex(Failure, 'diagnostic-encryption'):
            self.export(certificate=broken, certificate_sha256=hashlib.sha256(broken.read_bytes()).hexdigest())
        self.assertFalse(self.output.exists())

    def test_previous_branch_rerun_and_foreign_repository_cannot_export(self):
        for key, value in (('GITHUB_REF', 'refs/heads/b/qa-fresh45-monitor-20261028'),
                ('GITHUB_RUN_ATTEMPT', '2'), ('GITHUB_REPOSITORY', 'other/repository'),
                ('GITHUB_EVENT_NAME', 'workflow_dispatch'), ('GITHUB_SHA', 'bad')):
            with self.subTest(key=key), self.assertRaisesRegex(Failure, 'diagnostic-run-binding'):
                self.export(environment={**self.environment, key: value})
        self.assertFalse(self.output.exists())

    def test_wrong_origin_identity_profile_or_budget_cannot_export(self):
        for key, value in (('run', 124), ('attempt', True), ('head', 'd'*40), ('job', True),
                ('profile', 'fresh-firefox45-monitor-job-origin-20261028-v1'),
                ('platformMinutes', True), ('workDeadlineMinutes', 360), ('clockResetAllowed', True)):
            self.write('job-origin.json', json.dumps({**self.origin, key: value}).encode())
            with self.subTest(key=key), self.assertRaisesRegex(Failure, 'diagnostic-origin-binding'):
                self.export()
        self.assertFalse(self.output.exists())

    def test_missing_log_is_explicit_and_does_not_claim_capture_or_pass(self):
        (self.task/archive.SCOPED[0]).unlink()
        receipt = self.export()
        row = next(row for row in receipt['files'] if row['name'] == archive.SCOPED[0])
        self.assertEqual((row['status'], row['reason']), ('unavailable', 'missing'))
        self.assertIs(receipt['scopedFilesCaptured'], False)
        self.assertIs(receipt['nativeQualification'], False)
        self.assertNotIn(archive.SCOPED[0], self.members())

    def test_symlink_hardlink_and_fifo_are_never_followed_or_read(self):
        source = self.root/'foreign'
        source.write_bytes(b'FOREIGN-SENTINEL')
        raw = self.task/archive.SCOPED[0]
        for kind in ('symlink', 'hardlink', 'fifo'):
            raw.unlink()
            if kind == 'symlink':
                raw.symlink_to(source)
            elif kind == 'hardlink':
                os.link(source, raw)
            else:
                os.mkfifo(raw)
            payload, receipt = archive.snapshot(self.task, archive.bindings(self.environment))
            row = next(row for row in receipt['files'] if row['name'] == archive.SCOPED[0])
            self.assertEqual(row['reason'], 'file-kind')
            self.assertNotIn(b'FOREIGN-SENTINEL', payload)
            self.assertIs(receipt['scopedFilesCaptured'], False)

    def test_symlinked_directory_and_task_root_are_rejected(self):
        private = self.task/'mozilla-upstream-xpcom/native-private'
        old = private.with_name('old-private')
        private.rename(old)
        private.symlink_to(old, target_is_directory=True)
        payload, receipt = archive.snapshot(self.task, archive.bindings(self.environment))
        row = next(row for row in receipt['files'] if row['name'] == archive.SCOPED[0])
        self.assertEqual(row['reason'], 'directory-binding')
        self.assertNotIn(b'PRIVATE-RAW-SENTINEL', payload)
        link = self.root/'linked-task'
        link.symlink_to(self.task, target_is_directory=True)
        with self.assertRaises(Unknown):
            archive.snapshot(link, archive.bindings(self.environment))

    def test_mutating_open_file_is_detected_by_identity_verification(self):
        original = archive._FixedRead.read
        def change(reader, relative, cap):
            value = original(reader, relative, cap)
            if relative == archive.SCOPED[0]:
                self.write(relative, b'CHANGED-SENTINEL')
            return value
        with mock.patch.object(archive._FixedRead, 'read', change), self.assertRaises(Unknown):
            self.export()
        self.assertFalse(self.output.exists())

    def test_oversized_log_is_reported_without_truncation_or_capture(self):
        raw = self.task/archive.SCOPED[0]
        with raw.open('r+b') as stream:
            stream.truncate(16*archive.MIB+1)
        payload, receipt = archive.snapshot(self.task, archive.bindings(self.environment))
        row = next(row for row in receipt['files'] if row['name'] == archive.SCOPED[0])
        self.assertEqual((row['reason'], row['bytes']), ('size-limit', None))
        self.assertIs(receipt['scopedFilesCaptured'], False)

    def test_destination_is_exclusive_and_never_overwritten(self):
        self.output.write_bytes(b'OLD-SENTINEL')
        with self.assertRaisesRegex(Failure, 'diagnostic-destination'):
            self.export()
        self.assertEqual(self.output.read_bytes(), b'OLD-SENTINEL')
        self.output.unlink()
        self.output.symlink_to(self.root/'absent')
        with self.assertRaisesRegex(Failure, 'diagnostic-destination'):
            self.export()

    def test_untrusted_errors_and_crypto_stderr_never_appear_in_cli_output(self):
        args = ['diagnostic_archive.py', '--task', str(self.task), '--certificate', str(self.certificate),
            '--certificate-sha256', self.certificate_hash, '--output', str(self.output),
            '--openssl', self.openssl]
        with mock.patch('sys.argv', args), mock.patch.object(archive, 'export',
                side_effect=RuntimeError('PRIVATE-ERROR-SENTINEL')), \
                mock.patch('sys.stdout', new_callable=io.StringIO) as stdout:
            self.assertEqual(archive.main(), 1)
        result = json.loads(stdout.getvalue())
        self.assertNotIn('PRIVATE-', stdout.getvalue())
        self.assertEqual((result['status'], result['code']), ('unavailable', 'diagnostic-unavailable'))


if __name__ == '__main__':
    unittest.main()
