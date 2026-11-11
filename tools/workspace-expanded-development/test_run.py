import importlib.util
import json
from pathlib import Path
import unittest
from urllib.parse import quote

spec = importlib.util.spec_from_file_location('expanded', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)
original_spec = importlib.util.spec_from_file_location('six', Path(__file__).parents[1] / 'workspace-development/run.py')
original = importlib.util.module_from_spec(original_spec); original_spec.loader.exec_module(original)

class SeparateEvidence(unittest.TestCase):
    def events(self, group):
        config=runner.GROUPS[group]
        return [{'moduleName':Path(config['target']).name,'testName':name,'index':i+1,'total':len(config['names']),'status':status} for i,name in enumerate(config['names']) for status in ['running','passed']]
    def encode(self, events): return '\n'.join('[nora@workspace-case] '+quote(json.dumps(e),safe='') for e in events)
    def test_exact_separate_four_and_three(self):
        for group,count in [('preservation',4),('flows',3)]:
            self.assertEqual(len(runner.cases(self.encode(self.events(group)),group)),count*2)
    def test_missing_duplicate_failure_skip_cross_group_and_late_success_reject(self):
        for group in runner.GROUPS:
            for change in ['missing','duplicate','failed','skipped','foreign','wrong_count','wrong_order','late_success']:
                data=self.events(group)
                if change=='missing':data.pop()
                elif change=='duplicate':data.append(data[-1])
                elif change=='foreign':data[-1]['moduleName']='nativeLastTabWorkspacesEnabled.test.ts'
                elif change=='wrong_count':data[-1]['total']=6
                elif change=='wrong_order':data[0],data[-1]=data[-1],data[0]
                elif change=='late_success':data[1]['status']='failed'
                else:data[-1]['status']=change
                with self.subTest(group=group,change=change),self.assertRaises(ValueError):runner.cases(self.encode(data),group)
    def test_expanded_evidence_cannot_relax_original_six(self):
        for group in runner.GROUPS:
            with self.assertRaises(ValueError):original.cases(self.encode(self.events(group)))
        self.assertEqual(len(original.NAMES),6)
    def test_owner_identity_still_rejects_reused_dead_root(self):
        owned={10:'old'};table={10:{'start':'new','ppid':1,'session':10,'state':'S'},12:{'start':'foreign','ppid':10,'session':10,'state':'S'}}
        self.assertEqual(runner.adopt_owned(table,owned),set())
        self.assertEqual(owned,{10:'old'})

if __name__=='__main__':unittest.main()
