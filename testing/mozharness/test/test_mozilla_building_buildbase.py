import os
import unittest

from mozharness.base.log import LogMixin
from mozharness.base.script import ScriptMixin
from mozharness.mozilla.building.buildbase import MozconfigPathError, get_mozconfig_path


class FakeLogger(object):
    def log_message(self, *args, **kwargs):
        pass


class FakeScriptMixin(LogMixin, ScriptMixin, object):
    def __init__(self):
        self.script_obj = self
        self.log_obj = FakeLogger()


class TestMozconfigPath(unittest.TestCase):
    """
    Tests for :func:`get_mozconfig_path`.
    """

    def test_path(self):
        """
        Passing just ``src_mozconfig`` gives that file in ``abs_src_dir``.
        """
        script = FakeScriptMixin()

        abs_src_path = get_mozconfig_path(
            script,
            config={"src_mozconfig": "path/to/mozconfig"},
            dirs={"abs_src_dir": "/src"},
        )
        self.assertEqual(abs_src_path, "/src/path/to/mozconfig")

    def test_composite(self):
        """
        Passing ``app_name``, ``mozconfig_platform``, and ``mozconfig_variant``
        find the file in the ``config/mozconfigs`` subdirectory of that app
        directory.
        """
        script = FakeScriptMixin()

        config = {
            "app_name": "the-app",
            "mozconfig_variant": "variant",
            "mozconfig_platform": "platform9000",
        }
        abs_src_path = get_mozconfig_path(
            script,
            config=config,
            dirs={"abs_src_dir": "/src"},
        )
        self.assertEqual(
            abs_src_path,
            "/src/the-app/config/mozconfigs/platform9000/variant",
        )

    def test_manifest(self):
        """
        Passing just ``src_mozconfig_manifest`` looks in that file in
        ``abs_work_dir``, and finds the mozconfig file specified there in
        ``abs_src_dir``.
        """
        script = FakeScriptMixin()

        test_dir = os.path.dirname(__file__)
        config = {"src_mozconfig_manifest": "helper_files/mozconfig_manifest.json"}
        abs_src_path = get_mozconfig_path(
            script,
            config=config,
            dirs={
                "abs_src_dir": "/src",
                "abs_work_dir": test_dir,
            },
        )
        self.assertEqual(abs_src_path, "/src/path/to/mozconfig")

    def test_errors(self):
        script = FakeScriptMixin()

        configs = [
            # Not specifying any parts of a mozconfig path
            {},
            # Specifying both src_mozconfig and src_mozconfig_manifest
            {"src_mozconfig": "path", "src_mozconfig_manifest": "path"},
            # Specifying src_mozconfig with some or all of a composite
            # mozconfig path
            {
                "src_mozconfig": "path",
                "app_name": "app",
                "mozconfig_platform": "platform",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig": "path",
                "mozconfig_platform": "platform",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig": "path",
                "app_name": "app",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig": "path",
                "app_name": "app",
                "mozconfig_platform": "platform",
            },
            # Specifying src_mozconfig_manifest with some or all of a composite
            # mozconfig path
            {
                "src_mozconfig_manifest": "path",
                "app_name": "app",
                "mozconfig_platform": "platform",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig_manifest": "path",
                "mozconfig_platform": "platform",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig_manifest": "path",
                "app_name": "app",
                "mozconfig_variant": "variant",
            },
            {
                "src_mozconfig_manifest": "path",
                "app_name": "app",
                "mozconfig_platform": "platform",
            },
            # Specifying only some parts of a compsite mozconfig path
            {"mozconfig_platform": "platform", "mozconfig_variant": "variant"},
            {"app_name": "app", "mozconfig_variant": "variant"},
            {"app_name": "app", "mozconfig_platform": "platform"},
            {"app_name": "app"},
            {"mozconfig_variant": "variant"},
            {"mozconfig_platform": "platform"},
        ]

        for config in configs:
            with self.assertRaises(MozconfigPathError):
                get_mozconfig_path(script, config=config, dirs={})
