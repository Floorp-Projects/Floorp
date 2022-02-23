#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

from __future__ import absolute_import
import itertools
import json
import math
import os
import posixpath
import sys
import mozinfo
from manifestparser import TestManifest


class SingleTestMixin(object):
    """Utility functions for per-test testing like test verification and per-test coverage."""

    def __init__(self, **kwargs):
        super(SingleTestMixin, self).__init__(**kwargs)

        self.suites = {}
        self.tests_downloaded = False
        self.reftest_test_dir = None
        self.jsreftest_test_dir = None
        # Map from full test path on the test machine to a relative path in the source checkout.
        # Use self._map_test_path_to_source(test_machine_path, source_path) to add a mapping.
        self.test_src_path = {}
        self.per_test_log_index = 1

    def _map_test_path_to_source(self, test_machine_path, source_path):
        test_machine_path = test_machine_path.replace(os.sep, posixpath.sep)
        source_path = source_path.replace(os.sep, posixpath.sep)
        self.test_src_path[test_machine_path] = source_path

    def _is_gpu_suite(self, suite):
        if suite and (suite == "gpu" or suite.startswith("webgl")):
            return True
        return False

    def _find_misc_tests(self, dirs, changed_files, gpu=False):
        manifests = [
            (
                os.path.join(dirs["abs_mochitest_dir"], "tests", "mochitest.ini"),
                "mochitest-plain",
            ),
            (
                os.path.join(dirs["abs_mochitest_dir"], "chrome", "chrome.ini"),
                "mochitest-chrome",
            ),
            (
                os.path.join(
                    dirs["abs_mochitest_dir"], "browser", "browser-chrome.ini"
                ),
                "mochitest-browser-chrome",
            ),
            (
                os.path.join(dirs["abs_mochitest_dir"], "a11y", "a11y.ini"),
                "mochitest-a11y",
            ),
            (
                os.path.join(dirs["abs_xpcshell_dir"], "tests", "xpcshell.ini"),
                "xpcshell",
            ),
        ]
        is_fission = "fission.autostart=true" in self.config.get("extra_prefs", [])
        tests_by_path = {}
        all_disabled = []
        for (path, suite) in manifests:
            if os.path.exists(path):
                man = TestManifest([path], strict=False)
                active = man.active_tests(
                    exists=False, disabled=True, filters=[], **mozinfo.info
                )
                # Remove disabled tests. Also, remove tests with the same path as
                # disabled tests, even if they are not disabled, since per-test mode
                # specifies tests by path (it cannot distinguish between two or more
                # tests with the same path specified in multiple manifests).
                disabled = [t["relpath"] for t in active if "disabled" in t]
                all_disabled += disabled
                new_by_path = {
                    t["relpath"]: (suite, t.get("subsuite"), None)
                    for t in active
                    if "disabled" not in t and t["relpath"] not in disabled
                }
                tests_by_path.update(new_by_path)
                self.info(
                    "Per-test run updated with manifest %s (%d active, %d skipped)"
                    % (path, len(new_by_path), len(disabled))
                )

        ref_manifests = [
            (
                os.path.join(
                    dirs["abs_reftest_dir"],
                    "tests",
                    "layout",
                    "reftests",
                    "reftest.list",
                ),
                "reftest",
                "gpu",
            ),  # gpu
            (
                os.path.join(
                    dirs["abs_reftest_dir"],
                    "tests",
                    "testing",
                    "crashtest",
                    "crashtests.list",
                ),
                "crashtest",
                None,
            ),
        ]
        sys.path.append(dirs["abs_reftest_dir"])
        import manifest

        self.reftest_test_dir = os.path.join(dirs["abs_reftest_dir"], "tests")
        for (path, suite, subsuite) in ref_manifests:
            if os.path.exists(path):
                man = manifest.ReftestManifest()
                man.load(path)
                for t in man.tests:
                    relpath = os.path.relpath(t["path"], self.reftest_test_dir)
                    referenced = (
                        t["referenced-test"] if "referenced-test" in t else None
                    )
                    tests_by_path[relpath] = (suite, subsuite, referenced)
                    self._map_test_path_to_source(t["path"], relpath)
                self.info(
                    "Per-test run updated with manifest %s (%d tests)"
                    % (path, len(man.tests))
                )

        suite = "jsreftest"
        self.jsreftest_test_dir = os.path.join(
            dirs["abs_test_install_dir"], "jsreftest", "tests"
        )
        path = os.path.join(self.jsreftest_test_dir, "jstests.list")
        if os.path.exists(path):
            man = manifest.ReftestManifest()
            man.load(path)
            for t in man.files:
                # expect manifest test to look like:
                #    ".../tests/jsreftest/tests/jsreftest.html?test=test262/.../some_test.js"
                # while the test is in mercurial at:
                #    js/src/tests/test262/.../some_test.js
                epos = t.find("=")
                if epos > 0:
                    relpath = t[epos + 1 :]
                    test_path = os.path.join(self.jsreftest_test_dir, relpath)
                    relpath = os.path.join("js", "src", "tests", relpath)
                    self._map_test_path_to_source(test_path, relpath)
                    tests_by_path.update({relpath: (suite, None, None)})
                else:
                    self.warning("unexpected jsreftest test format: %s" % str(t))
            self.info(
                "Per-test run updated with manifest %s (%d tests)"
                % (path, len(man.files))
            )

        # for each changed file, determine if it is a test file, and what suite it is in
        for file in changed_files:
            # manifest paths use os.sep (like backslash on Windows) but
            # automation-relevance uses posixpath.sep
            file = file.replace(posixpath.sep, os.sep)
            entry = tests_by_path.get(file)
            if not entry:
                if file in all_disabled:
                    self.info("'%s' has been skipped on this platform." % file)
                if os.environ.get("MOZHARNESS_TEST_PATHS", None) is not None:
                    self.info("Per-test run could not find requested test '%s'" % file)
                continue

            if gpu and not self._is_gpu_suite(entry[1]):
                self.info(
                    "Per-test run (gpu) discarded non-gpu test %s (%s)"
                    % (file, entry[1])
                )
                continue
            elif not gpu and self._is_gpu_suite(entry[1]):
                self.info(
                    "Per-test run (non-gpu) discarded gpu test %s (%s)"
                    % (file, entry[1])
                )
                continue

            if is_fission and (
                (entry[0] == "mochitest-a11y") or (entry[0] == "mochitest-chrome")
            ):
                self.info(
                    "Per-test run (fission) discarded non-e10s test %s (%s)"
                    % (file, entry[0])
                )
                continue

            if entry[2] is not None:
                # Test name substitution, for reftest reference file handling:
                #  - if both test and reference modified, run the test file
                #  - if only reference modified, run the test file
                test_file = os.path.join(
                    os.path.dirname(file), os.path.basename(entry[2])
                )
                self.info("Per-test run substituting %s for %s" % (test_file, file))
                file = test_file

            self.info("Per-test run found test %s (%s/%s)" % (file, entry[0], entry[1]))
            subsuite_mapping = {
                # Map (<suite>, <subsuite>): <full-suite>
                #   <suite> is associated with a manifest, explicitly in code above
                #   <subsuite> comes from "subsuite" tags in some manifest entries
                #   <full-suite> is a unique id for the suite, matching desktop mozharness configs
                (
                    "mochitest-browser-chrome",
                    "a11y",
                    None,
                ): "mochitest-browser-a11y",
                (
                    "mochitest-browser-chrome",
                    "devtools",
                    None,
                ): "mochitest-devtools-chrome",
                ("mochitest-browser-chrome", "remote", None): "mochitest-remote",
                (
                    "mochitest-browser-chrome",
                    "screenshots",
                    None,
                ): "mochitest-browser-chrome-screenshots",  # noqa
                ("mochitest-plain", "media", None): "mochitest-media",
                # below should be on test-verify-gpu job
                ("mochitest-chrome", "gpu", None): "mochitest-chrome-gpu",
                ("mochitest-plain", "gpu", None): "mochitest-plain-gpu",
                ("mochitest-plain", "webgl1-core", None): "mochitest-webgl1-core",
                ("mochitest-plain", "webgl1-ext", None): "mochitest-webgl1-ext",
                ("mochitest-plain", "webgl2-core", None): "mochitest-webgl2-core",
                ("mochitest-plain", "webgl2-ext", None): "mochitest-webgl2-ext",
                ("mochitest-plain", "webgl2-deqp", None): "mochitest-webgl2-deqp",
                ("mochitest-plain", "webgpu", None): "mochitest-webgpu",
            }
            if entry in subsuite_mapping:
                suite = subsuite_mapping[entry]
            else:
                suite = entry[0]
            suite_files = self.suites.get(suite)
            if not suite_files:
                suite_files = []
            if file not in suite_files:
                suite_files.append(file)
            self.suites[suite] = suite_files

    def _find_wpt_tests(self, dirs, changed_files):
        # Setup sys.path to include all the dependencies required to import
        # the web-platform-tests manifest parser. web-platform-tests provides
        # the localpaths.py to do the path manipulation, which we load,
        # providing the __file__ variable so it can resolve the relative
        # paths correctly.
        paths_file = os.path.join(
            dirs["abs_wpttest_dir"], "tests", "tools", "localpaths.py"
        )
        with open(paths_file, "r") as f:
            exec(f.read(), {"__file__": paths_file})
        import manifest as wptmanifest

        tests_root = os.path.join(dirs["abs_wpttest_dir"], "tests")

        for extra in ("", "mozilla"):
            base_path = os.path.join(dirs["abs_wpttest_dir"], extra)
            man_path = os.path.join(base_path, "meta", "MANIFEST.json")
            man = wptmanifest.manifest.load(tests_root, man_path)
            self.info("Per-test run updated with manifest %s" % man_path)

            repo_tests_path = os.path.join("testing", "web-platform", extra, "tests")
            tests_path = os.path.join("tests", "web-platform", extra, "tests")
            for (type, path, test) in man:
                if type not in ["testharness", "reftest", "wdspec"]:
                    continue
                repo_path = os.path.join(repo_tests_path, path)
                # manifest paths use os.sep (like backslash on Windows) but
                # automation-relevance uses posixpath.sep
                repo_path = repo_path.replace(os.sep, posixpath.sep)
                if repo_path in changed_files:
                    self.info(
                        "Per-test run found web-platform test '%s', type %s"
                        % (path, type)
                    )
                    suite_files = self.suites.get(type)
                    if not suite_files:
                        suite_files = []
                    test_path = os.path.join(tests_path, path)
                    suite_files.append(test_path)
                    self.suites[type] = suite_files
                    self._map_test_path_to_source(test_path, repo_path)
                    changed_files.remove(repo_path)

        if os.environ.get("MOZHARNESS_TEST_PATHS", None) is not None:
            for file in changed_files:
                self.info(
                    "Per-test run could not find requested web-platform test '%s'"
                    % file
                )

    def find_modified_tests(self):
        """
        For each file modified on this push, determine if the modified file
        is a test, by searching test manifests. Populate self.suites
        with test files, organized by suite.

        This depends on test manifests, so can only run after test zips have
        been downloaded and extracted.
        """
        repository = os.environ.get("GECKO_HEAD_REPOSITORY")
        revision = os.environ.get("GECKO_HEAD_REV")
        if not repository or not revision:
            self.warning("unable to run tests in per-test mode: no repo or revision!")
            self.suites = {}
            self.tests_downloaded = True
            return

        def get_automationrelevance():
            response = self.load_json_url(url)
            return response

        dirs = self.query_abs_dirs()
        mozinfo.find_and_update_from_json(dirs["abs_test_install_dir"])
        e10s = self.config.get("e10s", False)
        mozinfo.update({"e10s": e10s})
        is_fission = "fission.autostart=true" in self.config.get("extra_prefs", [])
        mozinfo.update({"fission": is_fission})
        headless = self.config.get("headless", False)
        mozinfo.update({"headless": headless})
        if mozinfo.info["buildapp"] == "mobile/android":
            # extra android mozinfo normally comes from device queries, but this
            # code may run before the device is ready, so rely on configuration
            mozinfo.update(
                {"android_version": str(self.config.get("android_version", 24))}
            )
            mozinfo.update({"is_fennec": self.config.get("is_fennec", False)})
            mozinfo.update({"is_emulator": self.config.get("is_emulator", True)})
        mozinfo.update({"verify": True})
        self.info("Per-test run using mozinfo: %s" % str(mozinfo.info))

        # determine which files were changed on this push
        changed_files = set()
        url = "%s/json-automationrelevance/%s" % (repository.rstrip("/"), revision)
        contents = self.retry(get_automationrelevance, attempts=2, sleeptime=10)
        for c in contents["changesets"]:
            self.info(
                " {cset} {desc}".format(
                    cset=c["node"][0:12],
                    desc=c["desc"].splitlines()[0].encode("ascii", "ignore"),
                )
            )
            changed_files |= set(c["files"])
        changed_files = list(changed_files)

        # check specified test paths, as from 'mach try ... <path>'
        if os.environ.get("MOZHARNESS_TEST_PATHS", None) is not None:
            suite_to_paths = json.loads(os.environ["MOZHARNESS_TEST_PATHS"])
            specified_paths = itertools.chain.from_iterable(suite_to_paths.values())
            specified_paths = list(specified_paths)
            # filter the list of changed files to those found under the
            # specified path(s)
            changed_and_specified = set()
            for changed in changed_files:
                for specified in specified_paths:
                    if changed.startswith(specified):
                        changed_and_specified.add(changed)
                        break
            if changed_and_specified:
                changed_files = changed_and_specified
            else:
                # if specified paths do not match changed files, assume the
                # specified paths are explicitly requested tests
                changed_files = set()
                changed_files.update(specified_paths)
            self.info("Per-test run found explicit request in MOZHARNESS_TEST_PATHS:")
            self.info(str(changed_files))

        if self.config.get("per_test_category") == "web-platform":
            self._find_wpt_tests(dirs, changed_files)
        elif self.config.get("gpu_required", False) is not False:
            self._find_misc_tests(dirs, changed_files, gpu=True)
        else:
            self._find_misc_tests(dirs, changed_files)

        # per test mode run specific tests from any given test suite
        # _find_*_tests organizes tests to run into suites so we can
        # run each suite at a time

        # chunk files
        total_tests = sum([len(self.suites[x]) for x in self.suites])

        if total_tests == 0:
            self.warning("No tests to verify.")
            self.suites = {}
            self.tests_downloaded = True
            return

        files_per_chunk = total_tests / float(self.config.get("total_chunks", 1))
        files_per_chunk = int(math.ceil(files_per_chunk))

        chunk_number = int(self.config.get("this_chunk", 1))
        suites = {}
        start = (chunk_number - 1) * files_per_chunk
        end = chunk_number * files_per_chunk
        current = -1
        for suite in self.suites:
            for test in self.suites[suite]:
                current += 1
                if current >= start and current < end:
                    if suite not in suites:
                        suites[suite] = []
                    suites[suite].append(test)
            if current >= end:
                break

        self.suites = suites
        self.tests_downloaded = True

    def query_args(self, suite):
        """
        For the specified suite, return an array of command line arguments to
        be passed to test harnesses when running in per-test mode.

        Each array element is an array of command line arguments for a modified
        test in the suite.
        """
        # not in verify or per-test coverage mode: run once, with no additional args
        if not self.per_test_coverage and not self.verify_enabled:
            return [[]]

        files = []
        jsreftest_extra_dir = os.path.join("js", "src", "tests")
        # For some suites, the test path needs to be updated before passing to
        # the test harness.
        for file in self.suites.get(suite):
            if self.config.get("per_test_category") != "web-platform" and suite in [
                "reftest",
                "crashtest",
            ]:
                file = os.path.join(self.reftest_test_dir, file)
            elif (
                self.config.get("per_test_category") != "web-platform"
                and suite == "jsreftest"
            ):
                file = os.path.relpath(file, jsreftest_extra_dir)
                file = os.path.join(self.jsreftest_test_dir, file)

            if file is None:
                continue

            file = file.replace(os.sep, posixpath.sep)
            files.append(file)

        self.info("Per-test file(s) for '%s': %s" % (suite, files))

        args = []
        for file in files:
            cur = []

            cur.extend(self.coverage_args)
            cur.extend(self.verify_args)

            cur.append(file)
            args.append(cur)

        return args

    def query_per_test_category_suites(self, category, all_suites):
        """
        In per-test mode, determine which suites are active, for the given
        suite category.
        """
        suites = None
        if self.verify_enabled or self.per_test_coverage:
            if self.config.get("per_test_category") == "web-platform":
                suites = list(self.suites)
                self.info("Per-test suites: %s" % suites)
            elif all_suites and self.tests_downloaded:
                suites = dict(
                    (key, all_suites.get(key))
                    for key in self.suites
                    if key in all_suites.keys()
                )
                self.info("Per-test suites: %s" % suites)
            else:
                # Until test zips are downloaded, manifests are not available,
                # so it is not possible to determine which suites are active/
                # required for per-test mode; assume all suites from supported
                # suite categories are required.
                if category in ["mochitest", "xpcshell", "reftest"]:
                    suites = all_suites
        return suites

    def log_per_test_status(self, test_name, tbpl_status, log_level):
        """
        Log status of a single test. This will display in the
        Job Details pane in treeherder - a convenient summary of per-test mode.
        Special test name formatting is needed because treeherder truncates
        lines that are too long, and may remove duplicates after truncation.
        """
        max_test_name_len = 40
        if len(test_name) > max_test_name_len:
            head = test_name
            new = ""
            previous = None
            max_test_name_len = max_test_name_len - len(".../")
            while len(new) < max_test_name_len:
                head, tail = os.path.split(head)
                previous = new
                new = os.path.join(tail, new)
            test_name = os.path.join("...", previous or new)
            test_name = test_name.rstrip(os.path.sep)
        self.log(
            "TinderboxPrint: Per-test run of %s<br/>: %s" % (test_name, tbpl_status),
            level=log_level,
        )

    def get_indexed_logs(self, dir, test_suite):
        """
        Per-test tasks need distinct file names for the raw and errorsummary logs
        on each run.
        """
        index = ""
        if self.verify_enabled or self.per_test_coverage:
            index = "-test%d" % self.per_test_log_index
            self.per_test_log_index += 1
        raw_log_file = os.path.join(dir, "%s%s_raw.log" % (test_suite, index))
        error_summary_file = os.path.join(
            dir, "%s%s_errorsummary.log" % (test_suite, index)
        )
        return raw_log_file, error_summary_file
