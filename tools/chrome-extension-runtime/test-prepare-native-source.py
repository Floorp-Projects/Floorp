#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise incremental source safety with genuine Git patches and postimages."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("prepare_native_source", Path(__file__).with_name("prepare-native-source.py"))
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class IncrementalSourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="floorp-incremental-test-")
        self.root = Path(self.temporary.name)
        self.original = self.root / "original"
        self.original.mkdir()
        self.git(self.original, "init", "--quiet")
        for name, value in {"header.h": "#define VALUE 1\n", "unrelated.h": "untouched\n", "script.sh": "#!/bin/sh\necho baseline\n"}.items():
            (self.original / name).write_text(value)
        (self.original / "script.sh").chmod(0o755)
        self.git(self.original, "add", ".")
        self.git(self.original, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-qm", "baseline")
        self.revision = self.git(self.original, "rev-parse", "HEAD").strip()
        self.state = self.root / "state"
        self.state.mkdir()
        self.old = self.state / "old"
        self.old.mkdir()
        (self.original / "header.h").write_text("#define VALUE 2\n")
        (self.original / "script.sh").write_text("#!/bin/sh\necho patched\n")
        (self.original / "generated.cc").write_text("int generated = 1;\n")
        self.git(self.original, "add", ".")
        self.patch = self.old / "old.patch"
        self.patch.write_text(self.git(self.original, "diff", "HEAD"))
        self.git(self.original, "reset", "--hard", "--quiet", self.revision)
        self.source = self.root / "source"
        self.git(self.root, "clone", "--quiet", str(self.original), str(self.source))
        self.git(self.source, "apply", str(self.patch))
        self.previous = {"sourceCommit": self.revision, "snapshot": str(self.old),
                         "patches": {self.patch.name: hashlib.sha256(self.patch.read_bytes()).hexdigest()}}

    def tearDown(self):
        self.temporary.cleanup()

    @staticmethod
    def git(cwd, *arguments):
        return subprocess.check_output(["git", *arguments], cwd=cwd, text=True)

    def snapshot(self, name):
        value = self.state / name
        value.mkdir()
        return value

    def test_identical_postimages_preserve_header_mtimes_and_executable_mode(self):
        for name in ("header.h", "script.sh", "generated.cc", "unrelated.h"):
            os.utime(self.source / name, ns=(1_600_000_000_123456789, 1_600_000_000_123456789))
        before = {path.name: path.stat().st_mtime_ns for path in self.source.iterdir() if path.is_file()}
        images = PREPARE.incremental_prepare(self.original, self.source, self.revision,
                                            self.snapshot("same"), [self.patch], self.previous)
        self.assertEqual(before, {path.name: path.stat().st_mtime_ns for path in self.source.iterdir() if path.is_file()})
        self.assertTrue(images["script.sh"]["executable"])
        # The next invocation uses recorded file identities rather than having
        # to recreate the previous source generation from its patch snapshot.
        self.previous["files"] = images
        PREPARE.incremental_prepare(self.original, self.source, self.revision,
                                    self.snapshot("same-again"), [self.patch], self.previous)
        self.assertEqual(before["header.h"], (self.source / "header.h").stat().st_mtime_ns)

    def test_removed_patch_files_are_restored_or_deleted_with_exact_backups(self):
        (self.original / "header.h").write_text("#define VALUE 3\n")
        desired = self.root / "desired.patch"
        desired.write_text(self.git(self.original, "diff"))
        self.git(self.original, "reset", "--hard", "--quiet", self.revision)
        untouched = (self.source / "unrelated.h").stat().st_mtime_ns
        snapshot = self.snapshot("changed")
        images = PREPARE.incremental_prepare(self.original, self.source, self.revision, snapshot, [desired], self.previous)
        self.assertEqual((self.source / "header.h").read_text(), "#define VALUE 3\n")
        self.assertEqual((self.source / "script.sh").read_text(), "#!/bin/sh\necho baseline\n")
        self.assertTrue((self.source / "script.sh").stat().st_mode & 0o111)
        self.assertFalse((self.source / "generated.cc").exists())
        self.assertEqual((snapshot / "previous-postimages/generated.cc").read_text(), "int generated = 1;\n")
        self.assertEqual((snapshot / "previous-postimages/header.h").read_text(), "#define VALUE 2\n")
        self.assertEqual(set(images), {"header.h"})
        self.assertEqual(untouched, (self.source / "unrelated.h").stat().st_mtime_ns)

    def test_unexpected_tracked_and_generated_edits_fail_before_mutation(self):
        for name in ("unrelated.h", "header.h", "generated.cc"):
            with self.subTest(name=name):
                target = self.source / name
                expected = target.read_bytes()
                target.write_text("unexpected local edit\n")
                before = {path.name: path.read_bytes() for path in self.source.iterdir() if path.is_file()}
                with self.assertRaisesRegex(RuntimeError, "Unexpected tracked|differs from its maintained"):
                    PREPARE.incremental_prepare(self.original, self.source, self.revision,
                                                self.snapshot("refused-" + name), [self.patch], self.previous)
                self.assertEqual(before, {path.name: path.read_bytes() for path in self.source.iterdir() if path.is_file()})
                target.write_bytes(expected)

    def test_changed_prior_snapshot_is_not_accepted_as_authority(self):
        self.patch.write_text(self.patch.read_text() + "\n")
        before = (self.source / "header.h").read_bytes()
        with self.assertRaisesRegex(RuntimeError, "Prior patch snapshot changed"):
            PREPARE.incremental_prepare(self.original, self.source, self.revision,
                                        self.snapshot("snapshot-refused"), [self.patch], self.previous)
        self.assertEqual(before, (self.source / "header.h").read_bytes())


if __name__ == "__main__":
    unittest.main()
