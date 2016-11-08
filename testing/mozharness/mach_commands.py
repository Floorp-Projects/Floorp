# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import re
import subprocess
import sys
import urllib
import urlparse

import mozinfo

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase, MozbuildObject
from mozbuild.base import MachCommandConditions as conditions
from argparse import ArgumentParser

def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("suite_name", nargs=1, type=str, action="store",
                        help="Suite to run in mozharness")
    parser.add_argument("mozharness_args", nargs=argparse.REMAINDER,
                        help="Extra arguments to pass to mozharness")
    return parser

class MozharnessRunner(MozbuildObject):
    def __init__(self, *args, **kwargs):
        MozbuildObject.__init__(self, *args, **kwargs)


        self.test_packages_url = self._test_packages_url()
        self.installer_url = self._installer_url()

        desktop_unittest_config = [
            "--config-file", lambda: self.config_path("unittests",
                                                      "%s_unittest.py" % mozinfo.info['os']),
            "--config-file", lambda: self.config_path("developer_config.py")]

        self.config = {
            "__defaults__": {
                "config": ["--no-read-buildbot-config",
                           "--download-symbols", "ondemand",
                           "--installer-url", self.installer_url,
                           "--test-packages-url", self.test_packages_url]
            },

            "mochitest-valgrind": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mochitest-suite", "valgrind-plain"]
            },
            "mochitest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mochitest-suite", "plain"]
            },
            "mochitest-chrome": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mochitest-suite", "chrome"]
            },
            "mochitest-browser-chrome": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mochitest-suite", "browser-chrome"]
            },
            "mochitest-devtools-chrome": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mochitest-suite", "mochitest-devtools-chrome"]
            },
            "crashtest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--reftest-suite", "crashtest"]
            },
            "jsreftest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--reftest-suite", "jsreftest"]
            },
            "reftest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--reftest-suite", "reftest"]
            },
            "reftest-no-accel": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--reftest-suite", "reftest-no-accel"]
            },
            "cppunittest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--cppunittest-suite", "cppunittest"]
            },
            "xpcshell": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--xpcshell-suite", "xpcshell"]
            },
            "xpcshell-addons": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--xpcshell-suite", "xpcshell-addons"]
            },
            "jittest": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--jittest-suite", "jittest"]
            },
            "mozbase": {
                "script": "desktop_unittest.py",
                "config": desktop_unittest_config + [
                    "--mozbase-suite", "mozbase"]
            },
            "marionette": {
                "script": "marionette.py",
                "config": ["--config-file", self.config_path("marionette",
                                                             "test_config.py")]
            },
            "web-platform-tests": {
                "script": "web_platform_tests.py",
                "config": ["--config-file", self.config_path("web_platform_tests",
                                                             self.wpt_config)]
            },
        }


    def path_to_url(self, path):
        return urlparse.urljoin('file:', urllib.pathname2url(path))

    def _installer_url(self):
        package_re = {
            "linux": re.compile("^firefox-\d+\..+\.tar\.bz2$"),
            "win": re.compile("^firefox-\d+\..+\.installer\.exe$"),
            "mac": re.compile("^firefox-\d+\..+\.mac(?:64)?\.dmg$"),
        }[mozinfo.info['os']]
        dist_path = os.path.join(self.topobjdir, "dist")
        filenames = [item for item in os.listdir(dist_path) if
                     package_re.match(item)]
        assert len(filenames) == 1
        return self.path_to_url(os.path.join(dist_path, filenames[0]))

    def _test_packages_url(self):
        dist_path = os.path.join(self.topobjdir, "dist")
        filenames = [item for item in os.listdir(dist_path) if
                     item.endswith('test_packages.json')]
        assert len(filenames) == 1
        return self.path_to_url(os.path.join(dist_path, filenames[0]))

    def config_path(self, *parts):
        return self.path_to_url(os.path.join(self.topsrcdir, "testing", "mozharness",
                                             "configs", *parts))

    @property
    def wpt_config(self):
        return "test_config.py" if mozinfo.info['os'] != "win" else "test_config_windows.py"

    def run_suite(self, suite, **kwargs):
        default_config = self.config.get("__defaults__")
        suite_config = self.config.get(suite)

        if suite_config is None:
            print("Unknown suite %s" % suite)
            return 1

        script = os.path.join(self.topsrcdir, "testing", "mozharness",
                              "scripts", suite_config["script"])
        options = [item() if callable(item) else item
                   for item in default_config["config"] + suite_config["config"]]

        cmd = [script] + options

        rv = subprocess.call(cmd, cwd=os.path.dirname(script))
        return rv


@CommandProvider
class MozharnessCommands(MachCommandBase):
    @Command('mozharness', category='testing',
             description='Run tests using mozharness.',
             conditions=[conditions.is_firefox],
             parser=get_parser)
    def mozharness(self, **kwargs):
        runner = self._spawn(MozharnessRunner)
        return runner.run_suite(kwargs.pop("suite_name")[0], **kwargs)
