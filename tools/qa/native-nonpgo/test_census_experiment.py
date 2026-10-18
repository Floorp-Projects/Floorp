# SPDX-License-Identifier: MPL-2.0
import contextlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import census_experiment as ce
from support import Failure


class ExperimentCleanup(unittest.TestCase):
    def test_primary_unknown_receipt_survives_before_exception_propagates(self):
        value={}
        with mock.patch.object(ce,'owned_processes',side_effect=Failure('census-sysctl-permission')), \
                self.assertRaisesRegex(Failure,'census-sysctl-permission'):
            ce.take_owned(value)
        self.assertEqual(value,{'status':'unavailable','candidateIdentities':None})
        value={}
        def unavailable(_baseline,receipt,_observed):
            receipt.update(status='unavailable',newCandidateIdentities=None)
            raise Failure('census-sysctl-permission')
        with mock.patch.object(ce,'verify_owned_boundary',side_effect=unavailable), \
                self.assertRaisesRegex(Failure,'census-sysctl-permission'):
            ce.observe(set(),set(),value)
        self.assertEqual(value,{'status':'unavailable','newCandidateIdentities':None})

    def test_denied_cleanup_retains_original_failure_and_final_receipt(self):
        child=mock.Mock(returncode=None)
        child.poll.return_value=None
        child.terminate.side_effect=PermissionError('PRIVATE-TEXT')
        original={'stage':'known-child','code':'original'}
        runner=SimpleNamespace(failure=original,monitor_failure=None,records=[],elapsed=lambda:0)
        runner.fail=lambda *_args:None
        value={'status':'failed'}
        with tempfile.TemporaryDirectory() as temporary,contextlib.redirect_stdout(io.StringIO()) as output:
            task=Path(temporary)
            ce.finish_result(value,runner,task,child)
            receipt=json.loads((task/'census-experiment.json').read_text())
        self.assertEqual(receipt['firstFailure'],original)
        self.assertFalse(receipt['failureChildCleanup']['leaderReaped'])
        self.assertEqual(receipt['failureChildCleanup']['cleanupFailure'],{'operation':'terminate','kind':'permission'})
        self.assertNotIn('PRIVATE-TEXT',output.getvalue())
        child.kill.assert_not_called()

    def test_second_wait_timeout_is_retained_and_never_claims_reap(self):
        child=mock.Mock(returncode=None)
        child.poll.return_value=None
        child.wait.side_effect=subprocess.TimeoutExpired(['PRIVATE-ARGV'],5)
        with mock.patch.object(ce.Runner,'group_alive',return_value=False):
            value=ce.cleanup_child(child)
        self.assertFalse(value['leaderReaped'])
        self.assertEqual(value['cleanupFailure'],{'operation':'wait-after-kill','kind':'timeout'})
        self.assertNotIn('PRIVATE-ARGV',json.dumps(value))
        child.terminate.assert_called_once()
        child.kill.assert_called_once()

    def test_reaped_child_never_receives_another_signal(self):
        child=mock.Mock(returncode=0)
        child.poll.return_value=0
        with mock.patch.object(ce.Runner,'group_alive',return_value=False):
            value=ce.cleanup_child(child)
        self.assertTrue(value['leaderReaped'])
        self.assertEqual(value['groupCleanup'],'verified-gone')
        self.assertIsNone(value['cleanupFailure'])
        child.terminate.assert_not_called()
        child.kill.assert_not_called()
