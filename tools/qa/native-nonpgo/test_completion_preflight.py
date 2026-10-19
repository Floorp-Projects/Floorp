# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import completion_preflight as diagnostic
from support import Failure


class CompletionDriverGuards(unittest.TestCase):
    def exercise(self, *, stage_failure=False, receipt_error=False, skip=0, write_error=False,
                 mkdir=False, abi_error=None):
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
                def fail(self, stage, code):
                    if self.failure is None:
                        self.failure = {'stage':stage, 'code':code}
                def run(self, stage, *args, **kwargs):
                    path = task / (stage + '.private.log')
                    if stage == 'system-version':
                        path.write_text(plan['runner']['osVersion'])
                    if stage_failure and stage == 'completion-tests':
                        self.fail(stage, 'controlled-original-failure')
                        raise Failure('controlled-original-failure')
                    return path
            output = io.StringIO()
            env = {key:'fixed-binding' for key in diagnostic.BINDING_KEYS}
            env.update(TASK_ROOT=str(task), ImageVersion=plan['runner']['imageVersion'])
            with mock.patch.object(diagnostic, 'Runner', Runner), \
                    mock.patch.object(diagnostic, 'prepare_abi', side_effect=abi_error,
                                      return_value=task / 'abi.json'), \
                    mock.patch.object(diagnostic, 'test_receipt',
                        side_effect=ValueError('PRIVATE') if receipt_error else None,
                        return_value={'complete':True,'status':'passed','total':1,'skipped':skip}), \
                    mock.patch.object(diagnostic.sys, 'platform', 'darwin'), \
                    mock.patch.object(diagnostic.platform, 'machine', return_value='x86_64'), \
                    mock.patch.object(diagnostic.platform, 'python_version', return_value=plan['runner']['python']), \
                    mock.patch.dict(os.environ, env), \
                    mock.patch.object(diagnostic.sys, 'argv', ['diagnostic','--task',str(task),
                         '--expected-qa-tests','1','--expected-recording-tests','1']), \
                    contextlib.redirect_stdout(output):
                if write_error:
                    with mock.patch.object(diagnostic, 'write_json', side_effect=OSError('PRIVATE')):
                        code = diagnostic.main()
                else:
                    code = diagnostic.main()
            return code, json.loads(output.getvalue())['qaCompletionPreflight'], output.getvalue()

    def test_short_success_declares_changed_gate_and_unqualified_build(self):
        code, value, _ = self.exercise()
        self.assertEqual(code, 0)
        self.assertTrue(value['checkpointAdmissionChanged'])
        self.assertFalse(value['activeOwnedResourceLimitsChanged'])
        self.assertFalse(value['hostileSameUIDImmutabilityRequired'])
        self.assertEqual(value['fullBuildQualification'], 'NOT_TESTED')

    def test_primary_stage_failure_survives_receipt_and_persistence_failure(self):
        code, value, output = self.exercise(stage_failure=True, receipt_error=True, write_error=True)
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure'], {'stage':'completion-tests','code':'controlled-original-failure'})
        self.assertEqual(value['completion-tests']['status'], 'unavailable')
        self.assertNotIn('PRIVATE', output)

    def test_unavailable_skipped_or_unpersisted_success_is_nonzero(self):
        for changes in ({'receipt_error':True}, {'skip':1}, {'write_error':True}):
            with self.subTest(changes=changes):
                code, value, _ = self.exercise(**changes)
                self.assertEqual(code, 1)
                self.assertIsNotNone(value['firstFailure'])

    def test_early_failure_is_retained_and_unknown_error_text_is_private(self):
        code, value, _ = self.exercise(mkdir=True)
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['stage'], 'platform')
        code, value, output = self.exercise(abi_error=Failure('completion-preflight-PRIVATE'))
        self.assertEqual(code, 1)
        self.assertEqual(value['firstFailure']['code'], 'completion-preflight-stage-failure')
        self.assertNotIn('PRIVATE', output)


if __name__ == '__main__':
    unittest.main()
