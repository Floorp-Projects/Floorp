# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# flake8: noqa: E501

try:
    import cPickle as pickle
except ImportError:
    import pickle

import json
import os
import re
import shutil
import tempfile
from collections import defaultdict

import manifestupdate
import mozpack.path as mozpath
import mozunit
import pytest
from mozbuild.base import MozbuildObject
from mozbuild.frontend.reader import BuildReader
from mozbuild.test.common import MockConfig
from mozfile import NamedTemporaryFile
from moztest.resolve import (
    TEST_SUITES,
    BuildBackendLoader,
    TestManifestLoader,
    TestResolver,
)

here = os.path.abspath(os.path.dirname(__file__))
data_path = os.path.join(here, "data")


@pytest.fixture(scope="module")
def topsrcdir():
    return mozpath.join(data_path, "srcdir")


@pytest.fixture(scope="module")
def create_tests(topsrcdir):
    def inner(*paths, **defaults):
        tests = defaultdict(list)
        for path in paths:
            if isinstance(path, tuple):
                path, kwargs = path
            else:
                kwargs = {}

            path = mozpath.normpath(path)
            manifest_name = kwargs.get("flavor", defaults.get("flavor", "manifest"))
            manifest = kwargs.pop(
                "manifest",
                defaults.pop(
                    "manifest",
                    mozpath.join(mozpath.dirname(path), manifest_name + ".toml"),
                ),
            )

            manifest_abspath = mozpath.join(topsrcdir, manifest)
            relpath = mozpath.relpath(path, mozpath.dirname(manifest))
            test = {
                "name": relpath,
                "path": mozpath.join(topsrcdir, path),
                "relpath": relpath,
                "file_relpath": path,
                "flavor": "faketest",
                "dir_relpath": mozpath.dirname(path),
                "here": mozpath.dirname(manifest_abspath),
                "manifest": manifest_abspath,
                "manifest_relpath": manifest,
            }
            test.update(**defaults)
            test.update(**kwargs)

            # Normalize paths to ensure that the fixture matches reality.
            for k in [
                "ancestor_manifest",
                "manifest",
                "manifest_relpath",
                "path",
                "relpath",
            ]:
                p = test.get(k)
                if p:
                    test[k] = p.replace("/", os.path.sep)

            tests[path].append(test)

        # dump tests to stdout for easier debugging on failure
        print("The 'create_tests' fixture returned:")
        print(json.dumps(dict(tests), indent=2, sort_keys=True))
        return tests

    return inner


@pytest.fixture(scope="module")
def all_tests(create_tests):
    return create_tests(
        *[
            (
                "apple/test_a11y.html",
                {
                    "expected": "pass",
                    "manifest": "apple/a11y.toml",
                    "flavor": "a11y",
                },
            ),
            (
                "banana/currant/test_xpcshell_A.js",
                {
                    "firefox-appdir": "browser",
                    "flavor": "xpcshell",
                    "head": "head_global.js head_helpers.js head_http.js",
                },
            ),
            (
                "banana/currant/test_xpcshell_B.js",
                {
                    "firefox-appdir": "browser",
                    "flavor": "xpcshell",
                    "head": "head_global.js head_helpers.js head_http.js",
                },
            ),
            (
                "carrot/test_included.js",
                {
                    "ancestor_manifest": "carrot/xpcshell-one.toml",
                    "manifest": "carrot/xpcshell-shared.toml",
                    "flavor": "xpcshell",
                    "stick": "one",
                },
            ),
            (
                "carrot/test_included.js",
                {
                    "ancestor_manifest": "carrot/xpcshell-two.toml",
                    "manifest": "carrot/xpcshell-shared.toml",
                    "flavor": "xpcshell",
                    "stick": "two",
                },
            ),
            (
                "dragonfruit/elderberry/test_xpcshell_C.js",
                {
                    "flavor": "xpcshell",
                    "generated-files": "head_update.js",
                    "head": "head_update.js",
                    "manifest": "dragonfruit/xpcshell.toml",
                    "reason": "busted",
                    "run-sequentially": "Launches application.",
                    "skip-if": "os == 'android'",
                },
            ),
            (
                "dragonfruit/elderberry/test_xpcshell_C.js",
                {
                    "flavor": "xpcshell",
                    "generated-files": "head_update.js",
                    "head": "head_update.js head2.js",
                    "manifest": "dragonfruit/elderberry/xpcshell_updater.toml",
                    "reason": "don't work",
                    "run-sequentially": "Launches application.",
                    "skip-if": "os == 'android'",
                },
            ),
            (
                "fig/grape/src/TestInstrumentationA.java",
                {
                    "flavor": "instrumentation",
                    "manifest": "fig/grape/instrumentation.toml",
                    "subsuite": "background",
                },
            ),
            (
                "fig/huckleberry/src/TestInstrumentationB.java",
                {
                    "flavor": "instrumentation",
                    "manifest": "fig/huckleberry/instrumentation.toml",
                    "subsuite": "browser",
                },
            ),
            (
                "juniper/browser_chrome.js",
                {
                    "flavor": "browser-chrome",
                    "manifest": "juniper/browser.toml",
                    "skip-if": "e10s  # broken",
                },
            ),
            (
                "kiwi/browser_devtools.js",
                {
                    "flavor": "browser-chrome",
                    "manifest": "kiwi/browser.toml",
                    "subsuite": "devtools",
                    "tags": "devtools",
                },
            ),
        ]
    )


@pytest.fixture(scope="module")
def defaults(topsrcdir):
    def to_abspath(relpath):
        # test-defaults.pkl uses absolute paths with platform-specific path separators.
        # Use platform-specific separators if needed to avoid regressing on bug 1644223.
        return os.path.normpath(os.path.join(topsrcdir, relpath))

    return {
        (to_abspath("dragonfruit/elderberry/xpcshell_updater.toml")): {
            "support-files": "data/**\nxpcshell_updater.toml"
        },
        (
            to_abspath("carrot/xpcshell-one.toml"),
            to_abspath("carrot/xpcshell-shared.toml"),
        ): {
            "head": "head_one.js",
        },
        (
            to_abspath("carrot/xpcshell-two.toml"),
            to_abspath("carrot/xpcshell-shared.toml"),
        ): {
            "head": "head_two.js",
        },
    }


class WPTManifestNamespace(object):
    """Stand-in object for various WPT classes."""

    def __init__(self, *args):
        self.args = args

    def __hash__(self):
        return hash(str(self))

    def __eq__(self, other):
        return self.args == other.args

    def __iter__(self):
        yield tuple(self.args)


def fake_wpt_manifestupdate(topsrcdir, *args, **kwargs):
    with open(os.path.join(topsrcdir, "wpt_manifest_data.json")) as fh:
        data = json.load(fh)

    items = {}
    for tests_root, test_data in data.items():
        kwargs = {"tests_path": os.path.join(topsrcdir, tests_root)}

        for test_type, tests in test_data.items():
            for test in tests:
                obj = WPTManifestNamespace()
                if "mozilla" in tests_root:
                    obj.id = "/_mozilla/" + test
                else:
                    obj.id = "/" + test

                items[WPTManifestNamespace(test_type, test, {obj})] = kwargs
    return items


@pytest.fixture(params=[BuildBackendLoader, TestManifestLoader])
def resolver(request, tmpdir, monkeypatch, topsrcdir, all_tests, defaults):
    topobjdir = tmpdir.mkdir("objdir").strpath
    loader_cls = request.param

    if loader_cls == BuildBackendLoader:
        with open(os.path.join(topobjdir, "all-tests.pkl"), "wb") as fh:
            pickle.dump(all_tests, fh)
        with open(os.path.join(topobjdir, "test-defaults.pkl"), "wb") as fh:
            pickle.dump(defaults, fh)

        # The mock data already exists, so prevent BuildBackendLoader from regenerating
        # the build information from the whole gecko tree...
        class BuildBackendLoaderNeverOutOfDate(BuildBackendLoader):
            def backend_out_of_date(self, backend_file):
                return False

        loader_cls = BuildBackendLoaderNeverOutOfDate

    # Patch WPT's manifestupdate.run to return tests based on the contents of
    # 'data/srcdir/wpt_manifest_data.json'.
    monkeypatch.setattr(manifestupdate, "run", fake_wpt_manifestupdate)

    resolver = TestResolver(
        topsrcdir, None, None, topobjdir=topobjdir, loader_cls=loader_cls
    )
    resolver._puppeteer_loaded = True

    if loader_cls == TestManifestLoader:
        config = MockConfig(topsrcdir)
        resolver.load_tests.reader = BuildReader(config)
    return resolver


def test_load(resolver):
    assert len(resolver.tests_by_path) == 9

    assert len(resolver.tests_by_flavor["mochitest-plain"]) == 0
    assert len(resolver.tests_by_flavor["xpcshell"]) == 4
    assert len(resolver.tests_by_flavor["web-platform-tests"]) == 0

    assert len(resolver.tests_by_manifest) == 9

    resolver.add_wpt_manifest_data()
    assert len(resolver.tests_by_path) == 11
    assert len(resolver.tests_by_flavor["web-platform-tests"]) == 2
    assert len(resolver.tests_by_manifest) == 11
    assert "/html" in resolver.tests_by_manifest
    assert "/_mozilla/html" in resolver.tests_by_manifest


def test_resolve_all(resolver):
    assert len(list(resolver._resolve())) == 13


def test_resolve_filter_flavor(resolver):
    assert len(list(resolver._resolve(flavor="xpcshell"))) == 6


def test_resolve_by_dir(resolver):
    assert len(list(resolver._resolve(paths=["banana/currant"]))) == 2


def test_resolve_under_path(resolver):
    assert len(list(resolver._resolve(under_path="banana"))) == 2
    assert len(list(resolver._resolve(flavor="xpcshell", under_path="banana"))) == 2


def test_resolve_multiple_paths(resolver):
    result = list(resolver.resolve_tests(paths=["banana", "dragonfruit"]))
    assert len(result) == 4


def test_resolve_support_files(resolver):
    expected_support_files = "data/**\nxpcshell_updater.toml"
    tests = list(resolver.resolve_tests(paths=["dragonfruit"]))
    assert len(tests) == 2

    for test in tests:
        if test["manifest"].endswith("xpcshell_updater.toml"):
            assert test["support-files"] == expected_support_files
        else:
            assert "support-files" not in test


def test_resolve_path_prefix(resolver):
    tests = list(resolver._resolve(paths=["juniper"]))
    assert len(tests) == 1

    # relative manifest
    tests = list(resolver._resolve(paths=["apple/a11y.toml"]))
    assert len(tests) == 1
    assert tests[0]["name"] == "test_a11y.html"

    # absolute manifest
    tests = list(
        resolver._resolve(paths=[os.path.join(resolver.topsrcdir, "apple/a11y.toml")])
    )
    assert len(tests) == 1
    assert tests[0]["name"] == "test_a11y.html"


def test_cwd_children_only(resolver):
    """If cwd is defined, only resolve tests under the specified cwd."""
    # Pretend we're under '/services' and ask for 'common'. This should
    # pick up all tests from '/services/common'
    tests = list(
        resolver.resolve_tests(
            paths=["currant"], cwd=os.path.join(resolver.topsrcdir, "banana")
        )
    )

    assert len(tests) == 2

    # Tests should be rewritten to objdir.
    for t in tests:
        assert t["here"] == mozpath.join(
            resolver.topobjdir, "_tests/xpcshell/banana/currant"
        )


def test_various_cwd(resolver):
    """Test various cwd conditions are all equal."""
    expected = list(resolver.resolve_tests(paths=["banana"]))
    actual = list(resolver.resolve_tests(paths=["banana"], cwd="/"))
    assert actual == expected

    actual = list(resolver.resolve_tests(paths=["banana"], cwd=resolver.topsrcdir))
    assert actual == expected

    actual = list(resolver.resolve_tests(paths=["banana"], cwd=resolver.topobjdir))
    assert actual == expected


def test_subsuites(resolver):
    """Test filtering by subsuite."""
    tests = list(resolver.resolve_tests(paths=["fig"]))
    assert len(tests) == 2

    tests = list(resolver.resolve_tests(paths=["fig"], subsuite="browser"))
    assert len(tests) == 1
    assert tests[0]["name"] == "src/TestInstrumentationB.java"

    tests = list(resolver.resolve_tests(paths=["fig"], subsuite="background"))
    assert len(tests) == 1
    assert tests[0]["name"] == "src/TestInstrumentationA.java"

    # Resolve tests *without* a subsuite.
    tests = list(resolver.resolve_tests(flavor="browser-chrome", subsuite="undefined"))
    assert len(tests) == 1
    assert tests[0]["name"] == "browser_chrome.js"


def test_wildcard_patterns(resolver):
    """Test matching paths by wildcard."""
    tests = list(resolver.resolve_tests(paths=["fig/**"]))
    assert len(tests) == 2
    for t in tests:
        assert t["file_relpath"].startswith("fig")

    tests = list(resolver.resolve_tests(paths=["**/**.js", "apple/**"]))
    assert len(tests) == 9
    for t in tests:
        path = t["file_relpath"]
        assert path.startswith("apple") or path.endswith(".js")


def test_resolve_metadata(resolver):
    """Test finding metadata from outgoing files."""
    suites, tests = resolver.resolve_metadata(["bc"])
    assert suites == {"mochitest-browser-chrome"}
    assert tests == []

    suites, tests = resolver.resolve_metadata(
        ["mochitest-a11y", "/browser", "xpcshell"]
    )
    assert suites == {"mochitest-a11y", "xpcshell"}
    assert sorted(t["file_relpath"] for t in tests) == [
        "juniper/browser_chrome.js",
        "kiwi/browser_devtools.js",
    ]


def test_ancestor_manifest_defaults(resolver, topsrcdir, defaults):
    """Test that defaults from ancestor manifests are found."""
    tests = list(resolver._resolve(paths=["carrot/test_included.js"]))
    assert len(tests) == 2

    if tests[0]["ancestor_manifest"] == os.path.join("carrot", "xpcshell-one.toml"):
        [testOne, testTwo] = tests
    else:
        [testTwo, testOne] = tests

    assert testOne["ancestor_manifest"] == os.path.join("carrot", "xpcshell-one.toml")
    assert testOne["manifest_relpath"] == os.path.join("carrot", "xpcshell-shared.toml")
    assert testOne["head"] == "head_one.js"
    assert testOne["stick"] == "one"

    assert testTwo["ancestor_manifest"] == os.path.join("carrot", "xpcshell-two.toml")
    assert testTwo["manifest_relpath"] == os.path.join("carrot", "xpcshell-shared.toml")
    assert testTwo["head"] == "head_two.js"
    assert testTwo["stick"] == "two"


def test_task_regexes():
    """Test the task_regexes defined in TEST_SUITES."""
    task_labels = [
        "test-linux64/opt-marionette",
        "test-linux64/opt-mochitest-plain",
        "test-linux64/debug-mochitest-plain-e10s",
        "test-linux64/opt-mochitest-a11y",
        "test-linux64/opt-mochitest-browser",
        "test-linux64/opt-mochitest-browser-chrome",
        "test-linux64/opt-mochitest-browser-chrome-e10s",
        "test-linux64/opt-mochitest-browser-chrome-e10s-11",
        "test-linux64/opt-mochitest-chrome",
        "test-linux64/opt-mochitest-devtools",
        "test-linux64/opt-mochitest-devtools-chrome",
        "test-linux64/opt-mochitest-gpu",
        "test-linux64/opt-mochitest-gpu-e10s",
        "test-linux64/opt-mochitest-media-e10s-1",
        "test-linux64/opt-mochitest-media-e10s-11",
        "test-linux64/opt-mochitest-browser-screenshots-1",
        "test-linux64/opt-mochitest-browser-screenshots-e10s-1",
        "test-linux64/opt-reftest",
        "test-linux64/opt-geckoview-reftest",
        "test-linux64/debug-reftest-e10s-1",
        "test-linux64/debug-reftest-e10s-11",
        "test-linux64/opt-robocop",
        "test-linux64/opt-robocop-1",
        "test-linux64/opt-robocop-e10s",
        "test-linux64/opt-robocop-e10s-1",
        "test-linux64/opt-robocop-e10s-11",
        "test-linux64/opt-web-platform-tests-e10s-1",
        "test-linux64/opt-web-platform-tests-reftest-e10s-1",
        "test-linux64/opt-web-platform-tests-wdspec-e10s-1",
        "test-linux64/opt-web-platform-tests-1",
        "test-linux64/opt-web-platform-test-e10s-1",
        "test-linux64/opt-xpcshell",
        "test-linux64/opt-xpcshell-1",
        "test-linux64/opt-xpcshell-2",
    ]

    test_cases = {
        "mochitest-browser-chrome": [
            "test-linux64/opt-mochitest-browser-chrome",
            "test-linux64/opt-mochitest-browser-chrome-e10s",
        ],
        "mochitest-chrome": [
            "test-linux64/opt-mochitest-chrome",
        ],
        "mochitest-devtools-chrome": [
            "test-linux64/opt-mochitest-devtools-chrome",
        ],
        "mochitest-media": [
            "test-linux64/opt-mochitest-media-e10s-1",
        ],
        "mochitest-plain": [
            "test-linux64/opt-mochitest-plain",
            "test-linux64/debug-mochitest-plain-e10s",
        ],
        "mochitest-plain-gpu": [
            "test-linux64/opt-mochitest-gpu",
            "test-linux64/opt-mochitest-gpu-e10s",
        ],
        "mochitest-browser-screenshots": [
            "test-linux64/opt-mochitest-browser-screenshots-1",
            "test-linux64/opt-mochitest-browser-screenshots-e10s-1",
        ],
        "reftest": [
            "test-linux64/opt-reftest",
            "test-linux64/opt-geckoview-reftest",
            "test-linux64/debug-reftest-e10s-1",
        ],
        "robocop": [
            "test-linux64/opt-robocop",
            "test-linux64/opt-robocop-1",
            "test-linux64/opt-robocop-e10s",
            "test-linux64/opt-robocop-e10s-1",
        ],
        "web-platform-tests": [
            "test-linux64/opt-web-platform-tests-e10s-1",
            "test-linux64/opt-web-platform-tests-1",
        ],
        "web-platform-tests-reftest": [
            "test-linux64/opt-web-platform-tests-reftest-e10s-1",
        ],
        "web-platform-tests-wdspec": [
            "test-linux64/opt-web-platform-tests-wdspec-e10s-1",
        ],
        "xpcshell": [
            "test-linux64/opt-xpcshell",
            "test-linux64/opt-xpcshell-1",
        ],
    }

    regexes = []

    def match_task(task):
        return any(re.search(pattern, task) for pattern in regexes)

    for suite, expected in sorted(test_cases.items()):
        print(suite)
        regexes = TEST_SUITES[suite]["task_regex"]
        assert set(filter(match_task, task_labels)) == set(expected)


if __name__ == "__main__":
    mozunit.main()
