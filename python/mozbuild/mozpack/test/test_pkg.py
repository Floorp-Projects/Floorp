# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path
from string import Template
from unittest.mock import patch

import mozunit

import mozpack.pkg
from mozpack.pkg import (
    create_bom,
    create_payload,
    create_pkg,
    get_app_info_plist,
    get_apple_template,
    get_relative_glob_list,
    save_text_file,
    xar_package_folder,
)
from mozpack.test.test_files import TestWithTmpDir


class TestPkg(TestWithTmpDir):
    maxDiff = None

    class MockSubprocessRun:
        stderr = ""
        stdout = ""
        returncode = 0

        def __init__(self, returncode=0):
            self.returncode = returncode

    def _mk_test_file(self, name, mode=0o777):
        tool = Path(self.tmpdir) / f"{name}"
        tool.touch()
        tool.chmod(mode)
        return tool

    def test_get_apple_template(self):
        tmpl = get_apple_template("Distribution.template")
        assert type(tmpl) == Template

    def test_get_apple_template_not_file(self):
        with self.assertRaises(Exception):
            get_apple_template("tmpl-should-not-exist")

    def test_save_text_file(self):
        content = "Hello"
        destination = Path(self.tmpdir) / "test_save_text_file"
        save_text_file(content, destination)
        with destination.open("r") as file:
            assert content == file.read()

    def test_get_app_info_plist(self):
        app_path = Path(self.tmpdir) / "app"
        (app_path / "Contents").mkdir(parents=True)
        (app_path / "Contents/Info.plist").touch()
        data = {"foo": "bar"}
        with patch.object(mozpack.pkg.plistlib, "load", lambda x: data):
            assert data == get_app_info_plist(app_path)

    def test_get_app_info_plist_not_file(self):
        app_path = Path(self.tmpdir) / "app-does-not-exist"
        with self.assertRaises(Exception):
            get_app_info_plist(app_path)

    def _mock_payload(self, returncode):
        def _mock_run(*args, **kwargs):
            return self.MockSubprocessRun(returncode)

        return _mock_run

    def test_create_payload(self):
        destination = Path(self.tmpdir) / "mockPayload"
        with patch.object(mozpack.pkg.subprocess, "run", self._mock_payload(0)):
            create_payload(destination, Path(self.tmpdir), "cpio")

    def test_create_bom(self):
        bom_path = Path(self.tmpdir) / "Bom"
        bom_path.touch()
        root_path = Path(self.tmpdir)
        tool_path = Path(self.tmpdir) / "not-really-used-during-test"
        with patch.object(mozpack.pkg.subprocess, "check_call", lambda *x: None):
            create_bom(bom_path, root_path, tool_path)

    def get_relative_glob_list(self):
        source = Path(self.tmpdir)
        (source / "testfile").touch()
        glob = "*"
        assert len(get_relative_glob_list(source, glob)) == 1

    def test_xar_package_folder(self):
        source = Path(self.tmpdir)
        dest = source / "fakedestination"
        dest.touch()
        tool = source / "faketool"
        with patch.object(mozpack.pkg.subprocess, "check_call", lambda *x, **y: None):
            xar_package_folder(source, dest, tool)

    def test_xar_package_folder_not_absolute(self):
        source = Path("./some/relative/path")
        dest = Path("./some/other/relative/path")
        tool = source / "faketool"
        with patch.object(mozpack.pkg.subprocess, "check_call", lambda: None):
            with self.assertRaises(Exception):
                xar_package_folder(source, dest, tool)

    def test_create_pkg(self):
        def noop(*x, **y):
            pass

        def mock_get_app_info_plist(*args):
            return {"CFBundleShortVersionString": "1.0.0"}

        def mock_get_apple_template(*args):
            return Template("fake template")

        source = Path(self.tmpdir) / "FakeApp.app"
        source.mkdir()
        output = Path(self.tmpdir) / "output.pkg"
        fake_tool = Path(self.tmpdir) / "faketool"
        with patch.multiple(
            mozpack.pkg,
            get_app_info_plist=mock_get_app_info_plist,
            get_apple_template=mock_get_apple_template,
            save_text_file=noop,
            create_payload=noop,
            create_bom=noop,
            xar_package_folder=noop,
        ):
            create_pkg(source, output, fake_tool, fake_tool, fake_tool)


if __name__ == "__main__":
    mozunit.main()
