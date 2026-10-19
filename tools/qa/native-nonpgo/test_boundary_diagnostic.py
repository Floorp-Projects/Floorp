# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import boundary_diagnostic as diagnostic
from support import Failure


class DriverGuards(unittest.TestCase):
    def exercise(self, *, fault=None, mkdir=False, abi_error=None, write_error=False):
        with tempfile.TemporaryDirectory() as temporary:
            task = Path(temporary)
            (task / 'started').write_text('0')
            if mkdir:
                (task / 'tmp').mkdir()
            plan = json.loads((diagnostic.HERE / 'plan.json').read_text())
            class Runner:
                def __init__(self, *args):
                    self.failure = self.monitor_failure = None
                    self.monitor_diagnostics, self.records, self.groups = [], [], set()
                def elapsed(self):
                    return 1
                def fail(self, stage, code):
                    if self.failure is None:
                        self.failure = {'stage': stage, 'code': code}
                def run(self, stage, *args, **kwargs):
                    path = task / (stage + '.private.log')
                    if stage == 'system-version':
                        path.write_text(plan['runner']['osVersion'])
                    elif stage == 'task-api-observation':
                        value = {'status': 'completed', 'productionGateChanged': False,
                                 'observationOnly': True, 'checkpointEligibilityNotEvaluated': True,
                                 'actualDarwinExecution': True, 'experimentCompleted': True,
                                 'comparisonFallbackAuthority': False, 'resourceLimitsChanged': False,
                                 'repairEstablished': False, 'firstFailure': None}
                        if fault:
                            value.update(fault)
                        (task / 'task-api-experiment.json').write_text(json.dumps(value))
                    elif stage == 'readonly-boundary-observation':
                        value = {'status': 'completed', 'productionGateChanged': False,
                                 'diagnosticProcedure': 'completed', 'qualification': 'NO_GO',
                                 'productionGateReplacement': False,
                                 'checkpointEligibilityEstablished': False,
                                 'sameUIDImmutabilityEstablished': False, 'newArtifactsStored': 0}
                        (task / 'readonly-boundary-experiment.json').write_text(json.dumps(value))
                    return path
            env = {key: 'fixed-binding' for key in diagnostic.BINDING_KEYS}
            env.update(TASK_ROOT=str(task), ImageVersion=plan['runner']['imageVersion'])
            output = io.StringIO()
            with mock.patch.object(diagnostic, 'Runner', Runner), \
                    mock.patch.object(diagnostic, 'prepare_abi',
                                      side_effect=abi_error, return_value=task / 'abi.json'), \
                    mock.patch.object(diagnostic, 'test_receipt', return_value={
                        'complete': True, 'status': 'passed', 'total': 1, 'skipped': 0}), \
                    mock.patch.object(diagnostic.sys, 'platform', 'darwin'), \
                    mock.patch.object(diagnostic.platform, 'machine', return_value='x86_64'), \
                    mock.patch.object(diagnostic.platform, 'python_version',
                                      return_value=plan['runner']['python']), \
                    mock.patch.dict(os.environ, env), \
                    mock.patch.object(diagnostic.sys, 'argv',
                                      ['diagnostic', '--task', str(task), '--expected-tests', '1']), \
                    contextlib.redirect_stdout(output):
                if write_error:
                    with mock.patch.object(diagnostic, 'write_json', side_effect=OSError('PRIVATE')):
                        code = diagnostic.main()
                else:
                    code = diagnostic.main()
            value = json.loads(output.getvalue())['boundaryDiagnostic']
            return code, value, output.getvalue()

    def test_completed_diagnostics_never_establish_eligibility(self):
        code, value, _ = self.exercise()
        self.assertEqual(code, 0)
        self.assertEqual(value['status'], 'passed')
        self.assertFalse(value['productionGateChanged'])
        self.assertFalse(value['checkpointEligibilityEstablished'])
        self.assertEqual(value['longExecution'], 'ON_HOLD')

    def test_early_directory_failure_still_records_first_stage(self):
        code, value, _ = self.exercise(mkdir=True)
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['stage'], 'platform')

    def test_unknown_prefixed_exception_is_not_published(self):
        code, value, output = self.exercise(abi_error=Failure('boundary-diagnostic-PRIVATE'))
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['code'], 'boundary-diagnostic-stage-failed')
        self.assertNotIn('PRIVATE', output)

    def test_zero_exit_subprocess_cannot_claim_repair_or_change_resource_contract(self):
        for fault in ({'repairEstablished': True}, {'resourceLimitsChanged': True},
                      {'status': 'failed'}, {'comparisonFallbackAuthority': True},
                      {'firstFailure': {'code': 'fixed-failure'}}):
            with self.subTest(fault=fault):
                code, value, _ = self.exercise(fault=fault)
                self.assertEqual(code, 1)
                self.assertEqual(value['firstFailure']['code'], 'boundary-diagnostic-receipt-contract')

    def test_evidence_write_error_preserves_primary_and_prints_safe_receipt(self):
        code, value, output = self.exercise(abi_error=Failure('PRIVATE'), write_error=True)
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['stage'], 'abi')
        self.assertTrue(value['evidenceWriteFailure'])
        self.assertNotIn('PRIVATE', output)

    def test_evidence_write_error_has_a_cause_even_after_successful_observation(self):
        code, value, _ = self.exercise(write_error=True)
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['stage'], 'evidence-write')
        self.assertEqual(value['status'], 'failed')


if __name__ == '__main__':
    unittest.main()
