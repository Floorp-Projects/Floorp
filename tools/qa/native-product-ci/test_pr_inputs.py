# SPDX-License-Identifier: MPL-2.0
import copy
import json
from pathlib import Path
import unittest
import pr_inputs as p

class PinTests(unittest.TestCase):
    def setUp(self):
        repo={'full_name':p.REPOSITORY,'fork':False}
        self.event={'number':3138,'action':'synchronize','pull_request':{'head':{'repo':repo.copy(),'ref':p.BRANCH,'sha':'a'*40},'base':{'repo':repo.copy(),'ref':'main'}}}
        self.lock={'schemaVersion':1,'source':{'repository':p.RUNTIME,'commit':'b'*40,'tree':'c'*40},'artifacts':[{'platform':'linux','architecture':'x86_64','version':'160.0.1'}]}
    def select(self,**kw):return p.select(self.event,kw.get('name','pull_request'),kw.get('repo',p.REPOSITORY),kw.get('sha','a'*40),json.dumps(self.lock).encode())
    def fails(self,**kw):
        with self.assertRaises(ValueError):self.select(**kw)
    def test_exact_head_and_lock(self):
        result=self.select();self.assertEqual(result['floorp_sha'],'a'*40);self.assertEqual(result['runtime_sha'],'b'*40);self.assertEqual(result['runtime_version'],'160.0.1')
    def test_fork_false_even_same_name(self):self.event['pull_request']['head']['repo']['fork']=True;self.fails()
    def test_fork_unknown(self):del self.event['pull_request']['head']['repo']['fork'];self.fails()
    def test_foreign_repo(self):self.event['pull_request']['head']['repo']['full_name']='other/Floorp';self.fails()
    def test_foreign_event_repo(self):self.fails(repo='other/Floorp')
    def test_wrong_branch(self):self.event['pull_request']['head']['ref']='other';self.fails()
    def test_wrong_target(self):self.event['pull_request']['base']['ref']='release';self.fails()
    def test_wrong_base_repository(self):self.event['pull_request']['base']['repo']['full_name']='other/Floorp';self.fails()
    def test_wrong_pr(self):self.event['number']=3139;self.fails()
    def test_pull_request_target_rejected(self):self.fails(name='pull_request_target')
    def test_merge_sha_rejected(self):self.fails(sha='d'*40)
    def test_unexpected_action(self):self.event['action']='edited';self.fails()
    def test_missing_lock_commit(self):del self.lock['source']['commit'];self.fails()
    def test_mutable_lock_ref_rejected(self):self.lock['source']['commit']='nora-0.2.0';self.fails()
    def test_unapproved_runtime_repository(self):self.lock['source']['repository']='other/Runtime';self.fails()
    def test_missing_linux_version(self):self.lock['artifacts']=[];self.fails()
    def test_duplicate_linux_versions(self):self.lock['artifacts']*=2;self.fails()
    def test_missing_tree(self):del self.lock['source']['tree'];self.fails()

if __name__=='__main__':unittest.main()
