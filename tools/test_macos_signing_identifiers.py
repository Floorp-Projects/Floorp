import argparse
import ast
import glob
import logging
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock


ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path("tools/signing/macos/mach_commands.py")
RUNTIME_ROOT = None


def signing_function(source):
    function = next(
        node
        for node in ast.parse(source).body
        if isinstance(node, ast.FunctionDef) and node.name == "sign_with_rcodesign"
    )
    commands = []
    namespace = {
        "os": os,
        "glob": glob,
        "logging": logging,
        "should_skip_on_channel": lambda *_: False,
        "run": lambda ctx, cmd, **kwargs: commands.append(cmd[:]),
    }
    exec(
        compile(ast.Module(body=[function], type_ignores=[]), str(SOURCE), "exec"),
        namespace,
    )
    return namespace["sign_with_rcodesign"], commands


class HelperIdentifiersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory(prefix="floorp-signer-source-") as temp:
            target = Path(temp) / SOURCE
            target.parent.mkdir(parents=True)
            target.write_text(
                (RUNTIME_ROOT / SOURCE).read_text(encoding="utf-8"),
                encoding="utf-8",
                newline="\n",
            )
            upstream = (
                RUNTIME_ROOT
                / ".github/patches/upstream/tools-signing-macos-mach_commands.py.patch"
            )
            patch = (
                ROOT
                / ".github/patches/floorp-runtime/common/macos-helper-signing-identifiers.patch"
            )
            for candidate in (upstream, patch):
                subprocess.run(
                    ["git", "apply", "--check", str(candidate)], cwd=temp, check=True
                )
                subprocess.run(["git", "apply", str(candidate)], cwd=temp, check=True)
                if candidate == upstream:
                    cls.before_source = target.read_text(encoding="utf-8")
            cls.after_source = target.read_text(encoding="utf-8")

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="floorp-signer-app-")
        self.addCleanup(temp.cleanup)
        self.app = Path(temp.name) / "Floorp Test.app"
        self.directory = self.app / "Contents/Library/LaunchServices"
        self.directory.mkdir(parents=True)
        nested = (
            self.app / "Contents/MacOS/updater.app/Contents/MacOS/org.mozilla.updater"
        )
        nested.parent.mkdir(parents=True)
        nested.touch()

    def identifiers(self, source):
        sign, commands = signing_function(source)
        sign(
            Mock(),
            False,
            [{"globs": ["/"]}],
            "developer",
            "developer",
            "release",
            str(self.app),
            None,
            None,
        )
        command = commands[0]
        return [
            command[index + 1]
            for index, value in enumerate(command)
            if value == "--binary-identifier"
        ]

    def test_old_command_omits_identifiers_even_when_helpers_exist(self):
        (self.directory / "org.mozilla.updater").touch()
        self.assertEqual(self.identifiers(self.before_source), [])

    def test_existing_standalone_helpers_have_exact_scoped_identifiers(self):
        helpers = ("org.mozilla.updater", "org.mozilla.dmgInstallHelper")
        for name in helpers:
            (self.directory / name).touch()
        self.assertEqual(
            self.identifiers(self.after_source),
            [f"Contents/Library/LaunchServices/{name}:{name}" for name in helpers],
        )

    def test_absent_helpers_and_nested_bundle_are_not_overridden(self):
        self.assertEqual(self.identifiers(self.after_source), [])
        name = "org.mozilla.dmgInstallHelper"
        (self.directory / name).touch()
        self.assertEqual(
            self.identifiers(self.after_source),
            [f"Contents/Library/LaunchServices/{name}:{name}"],
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-root", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    RUNTIME_ROOT = options.runtime_root.resolve()
    unittest.main(argv=[__file__, *remaining])
