# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import gate
import resolve_native as r
import test_gate

class ResolverTests(unittest.TestCase):
    def test_native_dump_records(self):self.assertEqual(r.entries({'active_tests':[{'path':'/root/test_a.js'},{'path':'../test_b.js','disabled':'linux'}]}),['test_a.js','test_b.js'])
    def test_duplicate_basename_rejected(self):
        with self.assertRaises(ValueError):r.entries({'active_tests':[{'path':'a/test.js'},{'path':'b/test.js'}]})
    def test_empty_dump_rejected(self):
        with self.assertRaises(ValueError):r.entries({'active_tests':[]})
    def test_marionette_not_flattened(self):
        with self.assertRaises(ValueError):r.entries({'active_tests':[{'path':'test_a.py'}]})
    def test_unknown_dump_schema(self):
        with self.assertRaises(ValueError):r.entries({'active_tests':['test_a.js']})
    def test_pilot_excludes_only_marionette(self):
        self.assertEqual(set(gate.GROUPS)-set(gate.PILOT_GROUPS),{'p1-sessionstore-marionette'})

class PilotGateTests(unittest.TestCase):
    def test_pilot_never_satisfies_full_gate(self):
        fixture=test_gate.GateTests();fixture.setUp()
        try:
            plan=fixture.plan;report=fixture.report
            plan['scope']='browser-xpcshell'
            for g in plan['groups']:g['disabled_test_ids']=[]
            plan['groups']=[g for g in plan['groups'] if g['id'] in gate.PILOT_GROUPS]
            report['groups']=[g for g in report['groups'] if g['id'] in gate.PILOT_GROUPS]
            report['plan_sha256']=fixture.plan_hash()
            with self.assertRaises(ValueError):gate.aggregate(plan,report,fixture.root,fixture.needs)
            result=gate.aggregate(plan,report,fixture.root,fixture.needs,'browser-xpcshell')
            self.assertEqual(result['status'],'PASS_BROWSER_XPCSHELL_PILOT_ONLY')
            self.assertEqual(result['groups'],7)
        finally:fixture.tearDown()
    def test_native_label_normalization_is_pilot_only(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'raw'
            p.write_text('\n'.join(json.dumps(x) for x in [
                {'action':'suite_start'},
                {'action':'test_start','test':'../../installed/test_a.js'},
                {'action':'test_end','test':'../../installed/test_a.js','status':'PASS'},
                {'action':'suite_end'}]))
            self.assertEqual(gate.raw_summary(p,{'test_a.js'},'unique-file-basename'),1)
            with self.assertRaises(ValueError):gate.raw_summary(p,{'test_a.js'})

if __name__=='__main__':unittest.main()
