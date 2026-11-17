# SPDX-License-Identifier: MPL-2.0
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import focused_qualification as q

class FocusedTests(unittest.TestCase):
    def test_unapproved_selection_is_blocked(self):
        with patch.object(q,"CASES",{"session":None}), self.assertRaisesRegex(ValueError,"BLOCKED"):
            q.resolve("/absent","/absent","a"*40,"b"*40)
        with patch.object(q,"CASES",{"session":None}), self.assertRaisesRegex(ValueError,"BLOCKED"):
            q.finalize({}, {}, "/absent")

    def fixture(self, root):
        # Synthetic only. Deliberately not a publishable native selection.
        cases={"tabs":("tabs/browser.toml","browser_tabs.js"),"session":("session/browser.toml","browser_session.js")}
        for ident,(manifest,name) in cases.items():
            folder=root/"support/mochitest/browser"/Path(manifest).parent
            folder.mkdir(parents=True)
            (folder/"browser.toml").write_text('["'+name+'"]\n')
            (folder/name).write_text("// synthetic test fixture")
            (root/(ident+".dump.json")).write_text(json.dumps({"active_tests":[{"path":str(folder/name)}]}))
        return cases

    def test_manifest_dump_binding(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);cases=self.fixture(root)
            with patch.object(q,"CASES",cases):
                plan=q.resolve(root/"support",root,"a"*40,"b"*40)
                self.assertEqual(len(plan["groups"]),2)
                dump=root/"session.dump.json";data=json.loads(dump.read_text())
                for bad in ([{**data["active_tests"][0],"disabled":"platform"}],[],[{"path":"/foreign"}]):
                    dump.write_text(json.dumps({"active_tests":bad}))
                    with self.assertRaises(ValueError):q.resolve(root/"support",root,"a"*40,"b"*40)

    def test_strict_native_results(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);cases=self.fixture(root)
            with patch.object(q,"CASES",cases):
                plan=q.resolve(root/"support",root,"a"*40,"b"*40)
                execution={"schema":"floorp-focused-native-execution-v1", "plan_sha256":hashlib.sha256(json.dumps(plan,sort_keys=True,separators=(",", ":")).encode()).hexdigest(),
                           "finished":True,"owned_cleanup_complete":True,"product_unchanged":True,"support_unchanged":True,"results":[]}
                for ident,(_,name) in cases.items():
                    log=root/(ident+".jsonl")
                    events=[{"action":"suite_start"},{"action":"test_start","test":name},{"action":"test_end","test":name,"status":"OK"},{"action":"suite_end"}]
                    log.write_text("\n".join(map(json.dumps,events)))
                    execution["results"].append({"id":ident,"exit_code":0,"raw_log":log.name,"raw_sha256":q.sha(log)})
                self.assertFalse(q.finalize(plan,execution,root)["required_check"])
                execution["results"][0]["exit_code"]=1
                with self.assertRaises(ValueError):q.finalize(plan,execution,root)
                execution["results"][0]["exit_code"]=0
                execution["owned_cleanup_complete"]=False
                with self.assertRaises(ValueError):q.finalize(plan,execution,root)

if __name__=="__main__":unittest.main()
