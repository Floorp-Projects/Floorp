# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import imp
import json
import os

import mozunit
import pytest

from tryselect.tasks import build, resolve_tests_by_suite

MOZHARNESS_SCRIPTS = {
    "android_emulator_unittest": {
        "class_name": "AndroidEmulatorTest",
        "configs": [
            "android/android_common.py",
        ],
        "xfail": [
            "cppunittest",
            "crashtest-qr",
            "gtest",
            "geckoview-junit",
            "jittest",
            "jsreftest",
            "reftest-qr",
        ],
    },
    "desktop_unittest": {
        "class_name": "DesktopUnittest",
        "configs": [
            "unittests/linux_unittest.py",
            "unittests/mac_unittest.py",
            "unittests/win_unittest.py",
        ],
        "xfail": [
            "cppunittest",
            "gtest",
            "jittest",
            "jittest-chunked",
            "jittest1",
            "jittest2",
            "jsreftest",
            "mochitest-valgrind-plain",
            "reftest-no-accel",
            "reftest-snapshot",
        ],
    },
}
"""A suite being listed in a script's `xfail` list  means it won't work
properly with MOZHARNESS_TEST_PATHS (the mechanism |mach try fuzzy <path>|
uses).
"""


def get_mozharness_test_paths(name):
    scriptdir = os.path.join(build.topsrcdir, "testing", "mozharness", "scripts")

    files = imp.find_module(name, [scriptdir])
    mod = imp.load_module("scripts.{}".format(name), *files)

    class_name = MOZHARNESS_SCRIPTS[name]["class_name"]
    cls = getattr(mod, class_name)
    return cls(require_config_file=False)._get_mozharness_test_paths


@pytest.fixture(scope="module")
def all_suites():
    from moztest.resolve import _test_flavors, _test_subsuites

    all_suites = []
    for flavor in _test_flavors:
        all_suites.append({"flavor": flavor, "srcdir_relpath": "test"})

    for flavor, subsuite in _test_subsuites:
        all_suites.append(
            {"flavor": flavor, "subsuite": subsuite, "srcdir_relpath": "test"}
        )

    return all_suites


def generate_suites_from_config(path):
    configdir = os.path.join(build.topsrcdir, "testing", "mozharness", "configs")

    parent, name = os.path.split(path)
    name = os.path.splitext(name)[0]

    files = imp.find_module("{}".format(name), [os.path.join(configdir, parent)])
    mod = imp.load_module("config.{}".format(name), *files)
    config = mod.config

    for category in sorted(config["suite_definitions"]):
        key = "all_{}_suites".format(category)
        if key not in config:
            yield category,
            continue

        for suite in sorted(config["all_{}_suites".format(category)]):
            yield category, suite


def generate_suites():
    for name, script in MOZHARNESS_SCRIPTS.items():
        seen = set()

        for path in script["configs"]:
            for suite in generate_suites_from_config(path):
                if suite in seen:
                    continue
                seen.add(suite)

                item = (name, suite)

                if suite[-1] in script["xfail"]:
                    item = pytest.param(item, marks=pytest.mark.xfail)

                yield item


def idfn(item):
    name, suite = item
    return "{}/{}".format(name, suite[-1])


@pytest.mark.parametrize("item", generate_suites(), ids=idfn)
def test_suites(item, patch_resolver, all_suites):
    """An integration test to make sure the suites returned by
    `tasks.resolve_tests_by_suite` match up with the names defined in
    mozharness.
    """
    patch_resolver([], all_suites)
    suites = resolve_tests_by_suite(["test"])
    os.environ["MOZHARNESS_TEST_PATHS"] = json.dumps(suites)

    name, suite = item
    func = get_mozharness_test_paths(name)
    assert func(*suite)


if __name__ == "__main__":
    mozunit.main()
