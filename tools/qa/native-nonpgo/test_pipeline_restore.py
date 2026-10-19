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
        self.public_restore = mock.Mock(side_effect=lambda value: value)
        self.restore_failure = False
        self.stop_at_full_build = True
        self.mozilla = mock.Mock(side_effect=self.mozilla_success)

    def mozilla_success(self, source, fixture, task, runner, env, deadline, macho, result):
        result.update(status='passed', actualCompiledDebug=True,
                      actualCurrentSourceUpstreamTest=True, intentionalFailureDetected=True,
                      allCasesExactReaped=True, allCasesGroupsGone=True,
                      cases=[{'status':'passed'}, {'status':'failed', 'expectationSatisfied':True},
                             {'status':'passed'}])

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
            if self.restore_failure:
                raise Failure('test-original-restore-failure')
            value = {'qualification': 'verified-origin-cross-run-checkpoint-replay-v1',
                     'replay': {'contract': 'verified-origin-cross-run-checkpoint-replay-v1'}}
            (self.task / 'checkpoint-restore.json').write_text(json.dumps(value))
        versions = {'version-clang': 'clang version 22.1.8',
                    'version-rust': 'rustc 1.95.0 (59807616e 2026-04-14)',
                    'version-node': 'v22.16.0', 'version-nasm': 'NASM version 3.02'}
        path.write_text(versions.get(stage, ''))
        if stage == 'full-build':
            if self.stop_at_full_build:
                raise Failure('test-stop-at-full-build')
            object_path = self.task / 'obj/widget/cocoa/MacWebAppService.o'
            object_path.parent.mkdir(parents=True)
            object_path.write_bytes(b'object')
        return path

    def invoke(self, *, restore_only=True, mozilla_only=False, sdk_source=None):
        argv = ['pipeline.py', '--source', str(self.source), '--fixture', str(self.root / 'fixture'),
                '--harness', str(self.root / 'harness'), '--task', str(self.task),
                '--checkpoint-role', 'consumer', '--replay-origin']
        if restore_only:
            argv += ['--restore-only']
        if mozilla_only:
            argv += ['--mozilla-only']
        if sdk_source is not None:
            argv += ['--sdk-source', str(sdk_source)]
        with mock.patch('sys.argv', argv), \
                mock.patch.dict(os.environ, {'CHECKPOINT_BUILD_DATE': '20261019035607'}), \
                mock.patch.object(pipeline, 'Runner', return_value=self.runner), \
                mock.patch.object(pipeline, 'preflight', return_value={'status': 'passed'}), \
                mock.patch.object(pipeline, 'prepare_harness', return_value={'verified': True}), \
                mock.patch.object(pipeline, 'prepare_source'), \
                mock.patch.object(pipeline, 'clean_environment', return_value={'PATH': '/usr/bin'}), \
                mock.patch.object(pipeline, 'verify_config', self.verify_config), \
                mock.patch.object(pipeline, 'macho', return_value={'verified': True}), \
                mock.patch('mozilla_harness_smoke.run_smoke', self.mozilla), \
                mock.patch.object(pipeline, 'public_restore', self.public_restore), \
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

    def test_public_projection_failure_is_attributed_and_blocks_build(self):
        self.public_restore.side_effect=Failure('checkpoint-public-stage-completion')
        code,report,_=self.invoke()
        self.assertEqual(code,1)
        self.assertEqual(report['firstFailure'],{'stage':'checkpoint-restore-public-evidence',
                                               'code':'checkpoint-public-stage-completion'})
        self.assertNotIn('full-build',[row['stage'] for row in self.runner.records])

    def test_restore_failure_preserves_primary_and_never_invokes_projection(self):
        self.restore_failure=True
        code,report,_=self.invoke()
        self.assertEqual(code,1)
        self.assertEqual(report['firstFailure'],{'stage':'checkpoint-restore',
                                               'code':'test-original-restore-failure'})
        self.public_restore.assert_not_called()

    def sdk_source_fixture(self):
        from sdk_source import load_sdk_source, acquisition_receipt
        path = Path(pipeline.HERE) / 'sdk_source.json'
        spec = load_sdk_source(Path(pipeline.HERE) / 'plan.json', path)
        target = self.task / 'sdk-download/receipt.json'
        target.parent.mkdir()
        target.write_text(json.dumps(acquisition_receipt(spec)))
        return path, spec, target

    def test_alternate_sdk_forwards_explicit_source_and_reports_actual_acquisition(self):
        path, spec, _ = self.sdk_source_fixture()
        code, report, _ = self.invoke(sdk_source=path)
        self.assertEqual(code, 0)
        command = next(call.args[1] for call in self.runner.run.call_args_list if call.args[0]=='sdk-materialize')
        self.assertEqual(command[-2:], ['--sdk-source', str(path.resolve())])
        self.assertEqual(report['sdk']['artifact'], spec['pin']['artifact'])
        self.assertEqual(report['sdk']['sourceRun'], spec['pin']['run'])
        self.assertEqual(report['sdk']['tarSHA256'], spec['pin']['tarSHA256'])
        self.assertNotEqual(report['sdk']['artifact'], json.loads((pipeline.HERE/'plan.json').read_text())['sdk']['artifact'])
        self.assertEqual(report['sdk']['acquisitionSource']['uncompressedTar'], spec['uncompressedTar'])

    def test_missing_alternate_sdk_receipt_cannot_qualify_restore(self):
        path, _, receipt = self.sdk_source_fixture()
        receipt.unlink()
        code, report, _ = self.invoke(sdk_source=path)
        self.assertEqual(code, 1)
        self.assertEqual(report['status'], 'failed')
        self.assertNotIn('sdk', report)
        self.assertIn('sdkEvidenceError', report)

    def test_foreign_alternate_sdk_receipt_cannot_claim_actual_source(self):
        path, _, receipt = self.sdk_source_fixture()
        value = json.loads(receipt.read_text())
        value['artifact'] = 12133086104
        receipt.write_text(json.dumps(value))
        code, report, _ = self.invoke(sdk_source=path)
        self.assertEqual(code, 1)
        self.assertNotIn('sdk', report)
        self.assertEqual(report['sdkEvidenceError'], 'Failure')

    def test_mozilla_only_finishes_real_build_then_has_separate_qualification(self):
        self.stop_at_full_build = False
        code, report, _ = self.invoke(restore_only=False, mozilla_only=True)
        self.assertEqual(code, 0)
        self.mozilla.assert_called_once()
        self.assertEqual(report['status'], 'mozilla-ready')
        self.assertEqual(report['qualification'], 'verified-origin-current-mozilla-xpcshell-smoke')
        self.assertEqual(report['stagePackage'], 'NOT_TESTED')
        self.assertEqual(report['nativeAuthentication'], 'NOT_TESTED')
        self.assertFalse(report['freshObjdir'])
        self.assertFalse(report['sameJob'])
        self.assertFalse(report['productQualification'])
        stages = [row['stage'] for row in self.runner.records]
        self.assertIn('full-build', stages)
        self.assertNotIn('stage-package', stages)
        self.assertNotIn('native-suite', stages)
        self.assertEqual(report['mozilla']['cases'][1]['status'], 'failed')

    def test_mozilla_case_error_preserves_failed_outcome(self):
        self.stop_at_full_build = False
        self.mozilla.side_effect = Failure('mozilla-smoke-case-failed')
        code, report, _ = self.invoke(restore_only=False, mozilla_only=True)
        self.assertEqual(code, 1)
        self.assertEqual(report['firstFailure'], {'stage':'mozilla-harness', 'code':'mozilla-smoke-case-failed'})
        self.assertEqual(report['status'], 'failed')

    def test_mozilla_missing_compiled_debug_proof_fails_closed(self):
        self.stop_at_full_build = False
        def incomplete(*args):
            self.mozilla_success(*args)
            args[-1]['actualCompiledDebug'] = False
        self.mozilla.side_effect = incomplete
        code, report, _ = self.invoke(restore_only=False, mozilla_only=True)
        self.assertEqual(code, 1)
        self.assertEqual(report['firstFailure']['code'], 'mozilla-smoke-proof-incomplete')

    def test_mozilla_only_cannot_be_combined_with_restore_only(self):
        with self.assertRaisesRegex(Failure, 'mozilla-smoke-consumer-role'):
            self.invoke(restore_only=True, mozilla_only=True)

    def test_mozilla_never_runs_after_full_build_failure(self):
        code, report, _ = self.invoke(restore_only=False, mozilla_only=True)
        self.assertEqual(code, 1)
        self.assertEqual(report['firstFailure']['stage'], 'full-build')
        self.mozilla.assert_not_called()

    def test_mozilla_monitor_failure_is_inconclusive_and_not_product_failure_evidence(self):
        report = {'status':'failed','monitorFailure':None,
                  'mozilla':{'monitorFailure':{'stage':'mozilla-xpcshell'}}}
        value = pipeline.validation_outcome(report)
        self.assertEqual(value['execution'], 'monitor-inconclusive')
        self.assertFalse(value['monitorFailureEstablishesProductFailure'])


if __name__ == '__main__':
    unittest.main()
