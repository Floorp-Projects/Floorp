# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest import mock

import pipeline
from support import Failure


class RestoreOnlyFlow(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.task = self.root / 'task'
        self.task.mkdir()
        self.source = self.root / 'source'
        self.source.mkdir()
        for filename, value in [('started', str(time.time())), ('started-monotonic', str(time.monotonic()))]:
            (self.task / filename).write_text(value)
        for name, value in [('monitor-preflight.json', {'status': 'passed'}),
                            ('toolchain-receipts.json', [{}] * 11), ('source.json', {'verified': True}),
                            ('effective-config.json', {'verified': True})]:
            (self.task / name).write_text(json.dumps(value))
        self.runner = mock.Mock()
        self.runner.failure = None
        self.runner.monitor_failure = None
        self.runner.monitor_diagnostics = []
        self.runner.native_cleanup = None
        self.runner.records = []
        self.runner.elapsed.return_value = 1
        self.runner.fail.side_effect = self.fail
        self.runner.run.side_effect = self.run_stage
        self.verify_config = mock.Mock()

    def fail(self, stage, code):
        if self.runner.failure is None:
            self.runner.failure = {'stage': stage, 'code': code}

    def run_stage(self, stage, command, cwd, env, *args, **kwargs):
        self.runner.records.append({'stage': stage})
        path = self.task / (stage + '.private.log')
        if stage == 'sdk-materialize':
            (self.task / 'sdk-materialized.json').write_text(json.dumps({'path': str(self.task / 'sdk')}))
        if stage == 'checkpoint-restore':
            self.assertIn('--replay-origin', command)
            value = {'qualification': 'verified-origin-cross-run-checkpoint-replay-v1',
                     'replay': {'contract': 'verified-origin-cross-run-checkpoint-replay-v1'}}
            (self.task / 'checkpoint-restore.json').write_text(json.dumps(value))
        versions = {'version-clang': 'clang version 22.1.8',
                    'version-rust': 'rustc 1.95.0 (59807616e 2026-04-14)',
                    'version-node': 'v22.16.0', 'version-nasm': 'NASM version 3.02'}
        path.write_text(versions.get(stage, ''))
        if stage == 'full-build':
            raise Failure('test-stop-at-full-build')
        return path

    def invoke(self, *, restore_only=True):
        argv = ['pipeline.py', '--source', str(self.source), '--fixture', str(self.root / 'fixture'),
                '--harness', str(self.root / 'harness'), '--task', str(self.task),
                '--checkpoint-role', 'consumer', '--replay-origin']
        if restore_only:
            argv += ['--restore-only']
        with mock.patch('sys.argv', argv), \
                mock.patch.dict(os.environ, {'CHECKPOINT_BUILD_DATE': '20261019035607'}), \
                mock.patch.object(pipeline, 'Runner', return_value=self.runner), \
                mock.patch.object(pipeline, 'preflight', return_value={'status': 'passed'}), \
                mock.patch.object(pipeline, 'prepare_harness', return_value={'verified': True}), \
                mock.patch.object(pipeline, 'prepare_source'), \
                mock.patch.object(pipeline, 'clean_environment', return_value={'PATH': '/usr/bin'}), \
                mock.patch.object(pipeline, 'verify_config', self.verify_config), \
                mock.patch('checkpoint.verify_restored_config') as restored_config, \
                contextlib.redirect_stdout(io.StringIO()):
            code = pipeline.main()
        return code, json.loads((self.task / 'summary.json').read_text()), restored_config

    def test_restore_only_verifies_actual_config_then_stops_before_full_build(self):
        code, report, restored = self.invoke()
        self.assertEqual(code, 0)
        restored.assert_called_once_with(self.task)
        self.verify_config.assert_called_once()
        self.assertEqual(report['status'], 'restore-ready')
        self.assertIs(report['resumedEffectiveConfigVerified'], True)
        self.assertEqual(report['fullHostXUL'], 'NOT_TESTED')
        self.assertEqual(report['nativeAuthentication'], 'NOT_TESTED')
        self.assertFalse(report['productQualification'])
        self.assertEqual(report['validationOutcome']['nativeAuthentication'], 'not-verified')
        self.assertNotIn('full-build', [row['stage'] for row in self.runner.records])

    def test_full_consumer_continues_to_full_build_when_restore_only_absent(self):
        code, report, _ = self.invoke(restore_only=False)
        self.assertEqual(code, 1)
        self.assertEqual(report['firstFailure']['stage'], 'full-build')
        self.assertEqual(report['firstFailure']['code'], 'test-stop-at-full-build')
        self.assertIn('full-build', [row['stage'] for row in self.runner.records])

    def test_bad_effective_config_cannot_become_restore_ready(self):
        self.verify_config.side_effect = Failure('test-actual-config-mismatch')
        code, report, _ = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['firstFailure']['code'], 'test-actual-config-mismatch')
        self.assertEqual(report['firstFailure']['stage'], 'effective-config-validation')
        self.assertNotIn('full-build', [row['stage'] for row in self.runner.records])

    def test_missing_proof_cannot_become_restore_ready(self):
        (self.task / 'monitor-preflight.json').unlink()
        code, report, _ = self.invoke()
        self.assertEqual(code, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['firstFailure']['code'], 'checkpoint-restore-proof-incomplete')


if __name__ == '__main__':
    unittest.main()
