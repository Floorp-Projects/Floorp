# SPDX-License-Identifier: MPL-2.0
import json
from pathlib import Path
import tempfile
import unittest
import gate

class MozlogContractTests(unittest.TestCase):
    def evaluate(self,events,disabled=()):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'log';p.write_text('\n'.join(json.dumps(e) for e in events))
            return gate.raw_summary(p,{'a','b'},disabled_ids=disabled)
    def base(self):
        return [{'action':'suite_start'}, {'action':'test_start','test':'a'},
                {'action':'test_end','test':'a','status':'PASS'},
                {'action':'test_start','test':'b'},
                {'action':'test_end','test':'b','status':'PASS'}, {'action':'suite_end'}]
    def test_missing_status_rejected_even_without_expected(self):
        es=self.base();del es[2]['status']
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_expected_fail_omission_matches_mozlog_contract(self):
        es=self.base();es[2]['status']='FAIL';self.assertEqual(self.evaluate(es),2)
    def test_unexpected_fail_with_expected_is_rejected(self):
        es=self.base();es[2].update(status='FAIL',expected='PASS')
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_expected_disabled_skip_positive(self):
        es=self.base();es[4]['status']='SKIP';self.assertEqual(self.evaluate(es,{'b'}),1)
    def test_enabled_test_silently_skipped_is_failure(self):
        es=self.base();es[4]['status']='SKIP'
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_disabled_test_run_instead_of_skip_is_failure(self):
        with self.assertRaises(ValueError):self.evaluate(self.base(),{'b'})
    def test_skip_without_start_rejected(self):
        es=self.base();es.pop(3);es[3]['status']='SKIP'
        with self.assertRaises(ValueError):self.evaluate(es,{'b'})
    def test_substatus_before_start_rejected(self):
        es=self.base();es.insert(1,{'action':'test_status','test':'a','subtest':'x','status':'PASS'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_substatus_after_end_rejected(self):
        es=self.base();es.insert(3,{'action':'test_status','test':'a','subtest':'x','status':'PASS'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_substatus_missing_status_rejected(self):
        es=self.base();es.insert(2,{'action':'test_status','test':'a','subtest':'x'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_substatus_missing_subtest_rejected(self):
        es=self.base();es.insert(2,{'action':'test_status','test':'a','status':'PASS'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_unknown_substatus_rejected(self):
        es=self.base();es.insert(2,{'action':'test_status','test':'a','subtest':'x','status':'OK'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_expected_subfailure_preserved(self):
        es=self.base();es.insert(2,{'action':'test_status','test':'a','subtest':'known','status':'FAIL'});self.assertEqual(self.evaluate(es),2)
    def test_unexpected_subfailure_rejected(self):
        es=self.base();es.insert(2,{'action':'test_status','test':'a','subtest':'bad','status':'FAIL','expected':'PASS'})
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_assert_end_not_clean_completion(self):
        es=self.base();es[2]['status']='ASSERT'
        with self.assertRaises(ValueError):self.evaluate(es)
    def test_expected_subtest_infrastructure_errors_rejected(self):
        for status in ('ERROR','TIMEOUT','ASSERT','PRECONDITION_FAILED'):
            es=self.base();es.insert(2,{'action':'test_status','test':'a','subtest':'infrastructure','status':status})
            with self.subTest(status=status), self.assertRaises(ValueError):self.evaluate(es)
    def test_invalid_expected_rejected(self):
        es=self.base();es[2]['expected']='MAGIC'
        with self.assertRaises(ValueError):self.evaluate(es)

if __name__=='__main__':unittest.main()
