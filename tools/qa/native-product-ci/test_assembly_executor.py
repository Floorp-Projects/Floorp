# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import tempfile
import os
import subprocess
import unittest
from unittest.mock import patch
import assembly_executor as a
import artifact_assembly_recipe as recipe
import assembly_layout as layout

WORKFLOW=Path(__file__).resolve().parents[3]/".github/workflows/package.yml"
class ExecutorTests(unittest.TestCase):
    def test_setup_is_split_not_candidate_sudo(self):
        contract=a.setup_contract(WORKFLOW,layout.digest(WORKFLOW))
        self.assertEqual(contract["worker_argv"],["./mach","--no-interactive","bootstrap","--application-choice","browser_artifact_mode"])
        self.assertEqual(contract["system_argv"][1],["apt","install","msitools","jq","-y"])
        with self.assertRaises(ValueError):a.setup_contract(WORKFLOW,"0"*64)
    def test_secret_environment_rejected_before_execution(self):
        with patch.dict(a.os.environ,{"GITHUB_TOKEN":"fixture"}),patch.object(a.os,"geteuid",return_value=1000):
            with self.assertRaisesRegex(ValueError,"credentials"):
                a.execute({},"/absent","/absent","/absent","/absent",None,build_id="20261117000000",deno="/absent")
    def test_actual_shell_uses_exact_bound_deno(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/"noraneko").mkdir()
            binary=root/"deno"
            binary.write_text('#!/bin/sh\nprintf "%s" "$0" > "$BINDING_RESULT"\n')
            binary.chmod(0o755)
            env={"PATH":a.deno_binding(binary),"DENO_EXECUTABLE_PATH":str(binary),"BINDING_RESULT":str(root/"result")}
            script=dict(recipe.extract(WORKFLOW))["Build Noraneko"]
            self.assertNotIn("REAL_DENO_PATH",script)
            self.assertNotIn("/opt/hostedtoolcache",script)
            subprocess.run(["/bin/bash","-euc",script],cwd=root,env=env,check=True,timeout=10)
            self.assertEqual((root/"result").read_text(),str(binary))
            for path in ("", ":/bin", "/bin:", ".:/bin", "/unapproved:/bin"):
                with self.assertRaises(ValueError):a.deno_binding(binary,path)
            with patch.object(a.shutil,"which",return_value="/other/deno"):
                with self.assertRaisesRegex(ValueError,"mismatch"):a.deno_binding(binary)
            env["DENO_EXECUTABLE_PATH"]="/different/deno"
            result=subprocess.run(["/bin/bash","-euc",script],cwd=root,env=env,timeout=10)
            self.assertNotEqual(result.returncode,0)
    def test_owned_sequence_and_integrity_boundary(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);runtime=root/"runtime";runtime.mkdir();(runtime/"mozconfig").write_text("fixture")
            xhtml=runtime/"obj-artifact-build-output/dist/bin/browser/chrome/browser/content/browser/browser.xhtml"
            xhtml.parent.mkdir(parents=True);xhtml.write_bytes(b"synthetic injection")
            primary=root/"primary";primary.write_bytes(b"fixture")
            deno=root/"deno";deno.write_text("#!/bin/sh\nexit 0\n");deno.chmod(0o755)
            steps=recipe.write(WORKFLOW,root/"recipe")
            record={"steps":steps,"workflow_sha256":layout.digest(WORKFLOW),"runtime_sha":"a"*40,"floorp_sha":"a"*40,"primary_sha256":layout.digest(primary),"paths":{"runtime":str(runtime),"floorp":str(runtime/"noraneko"),"primary":str(primary),"home":str(root/"home")}}
            calls=[]
            class IO:
                GIB=1024**3
                @staticmethod
                def fresh(p):p.mkdir();return p
                @staticmethod
                def write_json(p,v):p.write_text("fixture")
                @staticmethod
                def run_owned(argv,cwd,env,logs,name,timeout,**kw):
                    calls.append((name,argv,dict(env),timeout,kw));return {"fixture":name}
            class Reply:stdout=b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            with patch.dict(a.os.environ,{},clear=True),patch.object(a.os,"geteuid",return_value=1000),patch.object(layout,"protected"),patch.object(layout,"checked",return_value=Reply()),patch.object(a.shutil,"which",side_effect=lambda name,path: str(deno) if name=="deno" else "/fixture"),patch.object(a.artifact_assembly_recipe,"verify_product",return_value={"fixture":True}):
                result=a.execute(record,root/"recipe",WORKFLOW,root/"unpacked",root/"state",IO,build_id="20261117000000",deno=str(deno))
            self.assertEqual(len(calls),12)
            names=[c[0] for c in calls]
            self.assertEqual(names[3],"artifact-bootstrap")
            self.assertEqual(names[4],"phase-3")
            self.assertEqual(result["native_execution"],"NOT_RUN")
            self.assertTrue(all(c[3]>0 for c in calls))
            self.assertFalse(any("sudo" in c[1] or "apt" in c[1] for c in calls))
            self.assertEqual(calls[-1][2]["MOZCONFIG"],str(runtime/"mozconfig"))
            self.assertEqual(calls[0][2]["MOZ_BUILD_DATE"],"20261117000000")

if __name__=="__main__":unittest.main()
