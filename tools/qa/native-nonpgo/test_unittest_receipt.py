# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import tempfile
import unittest

from unittest_receipt import receipt


class UnittestReceiptGuards(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)/'unittest.private.log'

    def test_failure_identity_type_code_and_frames_survive_without_private_text(self):
        frame = Path(__file__).with_name('process_census.py').resolve()
        self.path.write_text('ERROR: test_real_mac_new_owned_process_is_observed '
            '(test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed)\n'
            'Traceback (most recent call last):\n'
            f'  File "{frame}", line 100, in decode\n'
            'support.Failure: census-sysctl-permission\n'
            'ERROR: test_caps_stop_before_ready_or_upload '
            '(test_checkpoint.CheckpointGuards.test_caps_stop_before_ready_or_upload)\n'
            'PermissionError: [Errno 13] PRIVATE-SENTINEL token=https://private.invalid/signed\n'
            'Ran 178 tests in 67.0s\nFAILED (errors=2)\n')
        value = receipt(self.path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['errors'], 2)
        self.assertEqual(value['cases'][0]['failureCode'], 'census-sysctl-permission')
        self.assertEqual(value['cases'][0]['frames'][0]['file'], 'process_census.py')
        self.assertEqual(value['cases'][1]['exceptionType'], 'PermissionError')
        self.assertEqual(value['cases'][1]['errno'], 13)
        import json
        for secret in ('PRIVATE-SENTINEL','private.invalid','token=',str(frame.parent)):
            self.assertNotIn(secret, json.dumps(value))

    def test_complete_success_and_skips_remain_distinguishable(self):
        for ending, skipped in (('OK',0),('OK (skipped=10)',10)):
            self.path.write_text('Ran 178 tests in 7.7s\n'+ending+'\n')
            value = receipt(self.path)
            self.assertTrue(value['complete'])
            self.assertEqual(value['total'],178)
            self.assertEqual(value['skipped'],skipped)

    def test_missing_summary_truncated_line_symlink_and_wrong_case_counts_are_unknown(self):
        for text in ('ERROR: private', 'Ran 178 tests in 1.0s\n',
                     'Ran 178 tests in 1.0s\nFAILED (errors=1)\n'):
            self.path.write_text(text)
            value = receipt(self.path)
            self.assertFalse(value['complete'])
            self.assertIsNone(value['errors'])
        self.path.unlink()
        self.path.symlink_to(Path(__file__).resolve())
        self.assertFalse(receipt(self.path)['complete'])

    def test_unknown_identifier_or_phase_is_never_published(self):
        import json
        for header in ('ERROR: PRIVATE_SENTINEL_PHASE (test_checkpoint.PRIVATE_SENTINEL_CLASS.PRIVATE_SENTINEL_METHOD)',
                       'ERROR: PRIVATE_SENTINEL_PHASE (test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed)'):
            self.path.write_text(header+'\nAssertionError: PRIVATE_SENTINEL_MESSAGE\n'
                                 'Ran 1 test in 1.0s\nFAILED (errors=1)\n')
            value = receipt(self.path)
            self.assertFalse(value['complete'])
            self.assertNotIn('PRIVATE_SENTINEL',json.dumps(value))

    def test_subtest_parameters_function_text_and_invalid_frame_lines_are_omitted(self):
        import json
        file = Path(__file__).with_name('checkpoint.py').resolve()
        self.path.write_text('ERROR: test_real_mac_new_owned_process_is_observed '
            '(test_checkpoint.CheckpointGuards.test_real_mac_new_owned_process_is_observed) '
            '(token="PRIVATE_SENTINEL_PARAMETER")\n'
            f'  File "{file}", line 1, in PRIVATE_SENTINEL_FUNCTION\n'
            f'  File "{file}", line 999999999999, in PRIVATE_SENTINEL_FUNCTION\n'
            'AssertionError: PRIVATE_SENTINEL_MESSAGE\nRan 1 test in 1.0s\nFAILED (errors=1)\n')
        value = receipt(self.path)
        self.assertTrue(value['complete'])
        self.assertEqual(value['cases'][0]['frames'],[{'file':'checkpoint.py','line':1}])
        self.assertNotIn('PRIVATE_SENTINEL',json.dumps(value))
