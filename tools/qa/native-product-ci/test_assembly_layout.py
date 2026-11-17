# SPDX-License-Identifier: MPL-2.0
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
import assembly_layout as a

WORKFLOW=Path(__file__).resolve().parents[3]/".github/workflows/package.yml"
class LayoutTests(unittest.TestCase):
    def source(self,root,name):
        p=root/name;p.mkdir();(p/"tracked").write_text(name)
        def git(*args):return subprocess.check_output(["git","-C",str(p),*args],stderr=subprocess.DEVNULL)
        git("init");git("add","tracked");git("-c","user.name=Test","-c","user.email=test@example.invalid","commit","-m","fixture")
        sha=git("rev-parse","HEAD").decode().strip()
        (p/"tracked").write_text("dirty source must remain untouched")
        (p/"untracked-secret-fixture").write_text("not exported")
        return p,sha
    def test_real_local_clones_and_official_recipe(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);r,rs=self.source(root,"runtime-source");f,fs=self.source(root,"floorp-source")
            primary=root/"primary.tar.xz";primary.write_bytes(b"synthetic authenticated input")
            out=root/"layout"
            record=a.prepare(r,f,rs,fs,WORKFLOW,a.digest(WORKFLOW),primary,a.digest(primary),out)
            self.assertEqual((out/"runtime/tracked").read_text(),"runtime-source")
            self.assertEqual((out/"runtime/noraneko/tracked").read_text(),"floorp-source")
            self.assertFalse((out/"runtime/untracked-secret-fixture").exists())
            self.assertEqual((r/"tracked").read_text(),"dirty source must remain untouched")
            self.assertEqual(len(record["steps"]),11)
            names=[x["name"] for x in record["steps"]]
            self.assertLess(names.index("Apply upstream patches"),names.index("Build"))
            self.assertLess(names.index("Build"),names.index("Inject Noraneko"))
            self.assertEqual(record["native_execution"],"NOT_RUN")
            self.assertEqual(a.digest(record["paths"]["primary"]),a.digest(primary))
            self.assertEqual(subprocess.check_output(["git","-C",str(out/"runtime"),"remote"]),b"")
            with self.assertRaises(ValueError):a.protected(out/"recipe/00.sh",os.getuid())
            with self.assertRaises(ValueError):a.prepare(r,f,rs,fs,WORKFLOW,a.digest(WORKFLOW),primary,a.digest(primary),out)
    def test_bad_digest_stops_before_staging(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);primary=root/"primary";primary.write_text("fixture")
            with self.assertRaisesRegex(ValueError,"recipe digest"):
                a.prepare(root/"runtime",root/"floorp","a"*40,"b"*40,WORKFLOW,"0"*64,primary,a.digest(primary),root/"output")
            self.assertFalse((root/"output").exists())

if __name__=="__main__":unittest.main()
