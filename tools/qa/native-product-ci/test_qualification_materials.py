# SPDX-License-Identifier: MPL-2.0
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import qualification_materials as q

class MaterialTests(unittest.TestCase):
    def test_disabled_pin_blocked(self):
        with self.assertRaisesRegex(ValueError,"BLOCKED"):
            q.stage("/absent","/absent","/absent",{"enabled":False})

    def test_credentials_blocked_before_input(self):
        with patch.dict(os.environ,{"RUNTIME_ARTIFACT_TOKEN":"synthetic-fixture"}), patch.object(os,"geteuid",return_value=1000):
            with self.assertRaisesRegex(ValueError,"secretless"):
                q.stage("/absent","/absent","/absent",{"enabled":True})

    def test_helper_exact_tree_not_just_index(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);scripts=root/".github/workflows/scripts";scripts.mkdir(parents=True)
            source=scripts/"qa3_hosted.py";source.write_text("# fixture")
            def git(*args):return subprocess.check_output(["git","-C",d,*args],stderr=subprocess.DEVNULL)
            git("init");git("add",".github");git("-c","user.name=Test","-c","user.email=test@example.invalid","commit","-m","fixture")
            pin={"runtime_sha":git("rev-parse","HEAD").decode().strip()}
            with patch.object(q.importlib,"import_module",return_value="fixture"):
                self.assertEqual(q.helper_modules(root,pin),("fixture",)*3)
                git("update-index","--assume-unchanged",str(source.relative_to(root)))
                source.write_text("# changed even though clean status")
                with self.assertRaisesRegex(ValueError,"helper bytes"):
                    q.helper_modules(root,pin)

if __name__=="__main__":unittest.main()
