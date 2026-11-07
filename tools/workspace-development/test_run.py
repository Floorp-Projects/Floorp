import importlib.util
import json
from pathlib import Path
import unittest
from urllib.parse import quote

spec = importlib.util.spec_from_file_location('workspace_development', Path(__file__).with_name('run.py'))
runner = importlib.util.module_from_spec(spec); spec.loader.exec_module(runner)

class CaseEvidence(unittest.TestCase):
    def events(self):
        return [{'moduleName':Path(runner.TARGET).name,'testName':name,'index':i+1,'total':6,'status':status} for i,name in enumerate(runner.NAMES) for status in ['running','passed']]
    def encode(self, events): return '\n'.join('[nora@workspace-case] '+quote(json.dumps(e),safe='') for e in events)
    def test_exact_six(self): self.assertEqual(len(runner.cases(self.encode(self.events()))),12)
    def test_missing_duplicate_failure_skip_and_foreign_name_reject(self):
        for change in ['missing','duplicate','failed','skipped','foreign']:
            data=self.events()
            if change=='missing': data.pop()
            elif change=='duplicate': data.append(data[-1])
            elif change=='foreign': data[-1]['testName']='foreign'
            else: data[-1]['status']=change
            with self.subTest(change=change), self.assertRaises(ValueError): runner.cases(self.encode(data))
    def test_reused_dead_root_session_does_not_adopt_foreign_processes(self):
        owned={10:'old'}
        table={10:{'start':'new','ppid':1,'session':10,'state':'S'},12:{'start':'foreign','ppid':10,'session':10,'state':'S'}}
        self.assertEqual(runner.adopt_owned(table,owned),set())
        self.assertEqual(owned,{10:'old'})
    def test_live_original_member_anchors_owned_orphans(self):
        owned={10:'old',11:'child'}
        table={11:{'start':'child','ppid':1,'session':10,'state':'S'},12:{'start':'grandchild','ppid':1,'session':10,'state':'S'}}
        self.assertEqual(runner.adopt_owned(table,owned),{11,12})

if __name__ == '__main__': unittest.main()
