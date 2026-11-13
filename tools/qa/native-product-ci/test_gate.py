# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import gate as g

class GateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.plan = {'schema': 1, 'groups': [{'id': s, 'resolved_test_ids': [s+'/test']} for s in g.GROUPS]}
        self.report = {'schema': 1, 'finished': True, 'plan_sha256': self.plan_hash(), 'groups': []}
        for s in g.GROUPS:
            self.write_log(s, [{'action':'suite_start'}, {'action':'test_start','test':s+'/test'}, {'action':'test_end','test':s+'/test','status':'PASS'}, {'action':'suite_end'}])
        self.needs = {'execute': {'result':'success'}}
    def tearDown(self): self.tmp.cleanup()
    def plan_hash(self): return hashlib.sha256(json.dumps(self.plan,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    def write_log(self, s, events):
        p=self.root/(s+'.jsonl');p.write_text('\n'.join(json.dumps(e) for e in events))
        row={'id':s,'exit_code':0,'raw_log':p.name,'raw_sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
        self.report['groups']=[x for x in self.report['groups'] if x['id']!=s]+[row]
    def run_gate(self):return g.aggregate(self.plan,self.report,self.root,self.needs)
    def fails(self):
        with self.assertRaises((ValueError,OSError)):self.run_gate()
    def test_complete_synthetic_contract(self):self.assertEqual(self.run_gate()['executed'],8)
    def test_github_needs_includes_outputs(self):
        self.needs['execute']['outputs'] = {}
        self.assertEqual(self.run_gate()['executed'], 8)
    def test_each_missing_group(self):
        for s in g.GROUPS:
            old=copy.deepcopy(self.report);self.report['groups']=[x for x in self.report['groups'] if x['id']!=s];self.fails();self.report=old
    def test_duplicate_result(self):self.report['groups'][-1]=self.report['groups'][0];self.fails()
    def test_missing_plan_group(self):self.plan['groups'].pop();self.report['plan_sha256']=self.plan_hash();self.fails()
    def test_unbound_plan(self):self.plan['extra']=1;self.fails()
    def test_skipped_failed_cancelled_job(self):
        for status in ['skipped','failure','cancelled','']:
            self.needs['execute']['result']=status;self.fails()
    def test_missing_job(self):self.needs={};self.fails()
    def test_native_nonzero(self):self.report['groups'][0]['exit_code']=1;self.fails()
    def test_boolean_exit_is_not_zero(self):self.report['groups'][0]['exit_code']=False;self.fails()
    def test_incomplete(self):self.report['finished']=False;self.fails()
    def test_cleanup_required(self):self.report['manual_cleanup_required']=True;self.fails()
    def test_interrupted(self):self.report['interrupted']=True;self.fails()
    def test_missing_log(self):(self.root/self.report['groups'][0]['raw_log']).unlink();self.fails()
    def test_tampered_log(self):(self.root/self.report['groups'][0]['raw_log']).write_text('tamper');self.fails()
    def test_unsafe_path(self):self.report['groups'][0]['raw_log']='../elsewhere';self.fails()
    def test_symlink(self):
        p=self.root/self.report['groups'][0]['raw_log'];value=p.read_text();p.unlink();other=self.root/'other';other.write_text(value);p.symlink_to(other);self.fails()
    def events(self, middle):
        s=g.GROUPS[0];self.write_log(s,[{'action':'suite_start'},*middle,{'action':'suite_end'}])
    def test_skip_only(self):self.events([{'action':'test_end','test':g.GROUPS[0]+'/test','status':'SKIP'}]);self.fails()
    def test_zero_tests(self):self.events([]);self.fails()
    def test_test_without_start(self):self.events([{'action':'test_end','test':g.GROUPS[0]+'/test','status':'PASS'}]);self.fails()
    def test_unexpected_status(self):self.events([{'action':'test_start','test':g.GROUPS[0]+'/test'},{'action':'test_end','test':g.GROUPS[0]+'/test','status':'FAIL','expected':'PASS'}]);self.fails()
    def test_expected_timeout_still_fails(self):self.events([{'action':'test_start','test':g.GROUPS[0]+'/test'},{'action':'test_end','test':g.GROUPS[0]+'/test','status':'TIMEOUT'}]);self.fails()
    def test_unplanned_test(self):self.events([{'action':'test_start','test':'foreign'}]);self.fails()
    def test_missing_test(self):self.plan['groups'][0]['resolved_test_ids'].append('another');self.report['plan_sha256']=self.plan_hash();self.fails()
    def test_expected_upstream_failure(self):self.events([{'action':'test_start','test':g.GROUPS[0]+'/test'},{'action':'test_end','test':g.GROUPS[0]+'/test','status':'FAIL'}]);self.assertEqual(self.run_gate()['executed'],8)
    def test_malformed_raw(self):
        p=self.root/self.report['groups'][0]['raw_log'];p.write_text('{');self.report['groups'][0]['raw_sha256']=hashlib.sha256(p.read_bytes()).hexdigest();self.fails()
    def test_missing_suite_end(self):self.write_log(g.GROUPS[0],[{'action':'suite_start'}]);self.fails()
    def test_native_error(self):self.events([{'action':'log','level':'ERROR'}]);self.fails()

class ProducerTests(unittest.TestCase):
    def setUp(self):
        self.a='a'*40;self.b='b'*40
        self.r={'schema':'floorp-native-product-ci/1','floorp_sha':self.a,'runtime_sha':self.b,'platform':'linux-x86_64','tests_enabled':True,'product_finalized':True,'cohort':{'repository':'Floorp-Projects/Floorp-Runtime','run_id':1,'run_attempt':2,'build_id':'20261113000000'},'manifests':dict.fromkeys(g.MANIFESTS,'c'*64),'artifacts':{}}
        for role in ['runtime','test_support','product']:
            self.r['artifacts'][role]={'sha256':'d'*64,'build_id':'20261113000000','runtime_sha':self.b,'floorp_sha':self.a,'producer_run_id':1,'producer_run_attempt':2}
    def check(self):return g.validate_producer(self.r,self.a,self.b)
    def fails(self):
        with self.assertRaises(ValueError):self.check()
    def test_valid_synthetic_cohort(self):self.check()
    def test_wrong_source(self):self.r['floorp_sha']='e'*40;self.fails()
    def test_missing_manifest(self):self.r['manifests'].pop(g.MANIFESTS[-1]);self.fails()
    def test_missing_support(self):del self.r['artifacts']['test_support'];self.fails()
    def test_mixed_attempt(self):self.r['artifacts']['test_support']['producer_run_attempt']=1;self.fails()
    def test_mixed_build(self):self.r['artifacts']['product']['build_id']='20261114000000';self.fails()
    def test_not_product(self):self.r['product_finalized']=False;self.fails()
    def test_runtime_only(self):self.r['artifacts']['product']['floorp_sha']='e'*40;self.fails()
    def test_short_sha(self):self.a='abc';self.fails()

if __name__=='__main__':unittest.main()
