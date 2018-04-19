#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import argparse
import os
import posixpath
import re
import sys
import mozinfo
from manifestparser import TestManifest
from mozharness.base.script import PostScriptAction


class SingleTestMixin(object):
    """Utility functions for per-test testing like test verification and per-test coverage."""

    def __init__(self):
        self.suites = {}
        self.tests_downloaded = False
        self.reftest_test_dir = None
        self.jsreftest_test_dir = None

    def _find_misc_tests(self, dirs, changed_files):
        manifests = [
            (os.path.join(dirs['abs_mochitest_dir'], 'tests', 'mochitest.ini'), 'plain'),
            (os.path.join(dirs['abs_mochitest_dir'], 'chrome', 'chrome.ini'), 'chrome'),
            (os.path.join(dirs['abs_mochitest_dir'], 'browser', 'browser-chrome.ini'), 'browser-chrome'),
            (os.path.join(dirs['abs_mochitest_dir'], 'a11y', 'a11y.ini'), 'a11y'),
            (os.path.join(dirs['abs_xpcshell_dir'], 'tests', 'xpcshell.ini'), 'xpcshell'),
        ]
        tests_by_path = {}
        for (path, suite) in manifests:
            if os.path.exists(path):
                man = TestManifest([path], strict=False)
                active = man.active_tests(exists=False, disabled=True, filters=[], **mozinfo.info)
                # Remove disabled tests. Also, remove tests with the same path as
                # disabled tests, even if they are not disabled, since per-test mode
                # specifies tests by path (it cannot distinguish between two or more
                # tests with the same path specified in multiple manifests).
                disabled = [t['relpath'] for t in active if 'disabled' in t]
                new_by_path = {t['relpath']:(suite,t.get('subsuite')) \
                               for t in active if 'disabled' not in t and \
                               t['relpath'] not in disabled}
                tests_by_path.update(new_by_path)
                self.info("Per-test run updated with manifest %s" % path)

        ref_manifests = [
            (os.path.join(dirs['abs_reftest_dir'], 'tests', 'layout', 'reftests', 'reftest.list'), 'reftest'),
            (os.path.join(dirs['abs_reftest_dir'], 'tests', 'testing', 'crashtest', 'crashtests.list'), 'crashtest'),
        ]
        sys.path.append(dirs['abs_reftest_dir'])
        import manifest
        self.reftest_test_dir = os.path.join(dirs['abs_reftest_dir'], 'tests')
        for (path, suite) in ref_manifests:
            if os.path.exists(path):
                man = manifest.ReftestManifest()
                man.load(path)
                tests_by_path.update({os.path.relpath(t,self.reftest_test_dir):(suite,None) for t in man.files})
                self.info("Per-test run updated with manifest %s" % path)

        suite = 'jsreftest'
        self.jsreftest_test_dir = os.path.join(dirs['abs_test_install_dir'], 'jsreftest', 'tests')
        path = os.path.join(self.jsreftest_test_dir, 'jstests.list')
        if os.path.exists(path):
            man = manifest.ReftestManifest()
            man.load(path)
            for t in man.files:
                # expect manifest test to look like:
                #    ".../tests/jsreftest/tests/jsreftest.html?test=test262/.../some_test.js"
                # while the test is in mercurial at:
                #    js/src/tests/test262/.../some_test.js
                epos = t.find('=')
                if epos > 0:
                    relpath = t[epos+1:]
                    relpath = os.path.join('js', 'src', 'tests', relpath)
                    tests_by_path.update({relpath:(suite,None)})
                else:
                    self.warning("unexpected jsreftest test format: %s" % str(t))
            self.info("Per-test run updated with manifest %s" % path)

        # for each changed file, determine if it is a test file, and what suite it is in
        for file in changed_files:
            # manifest paths use os.sep (like backslash on Windows) but
            # automation-relevance uses posixpath.sep
            file = file.replace(posixpath.sep, os.sep)
            entry = tests_by_path.get(file)
            if entry:
                self.info("Per-test run found test %s" % file)
                subsuite_mapping = {
                    ('browser-chrome', 'clipboard') : 'browser-chrome-clipboard',
                    ('chrome', 'clipboard') : 'chrome-clipboard',
                    ('plain', 'clipboard') : 'plain-clipboard',
                    ('browser-chrome', 'devtools') : 'mochitest-devtools-chrome',
                    ('browser-chrome', 'gpu') : 'browser-chrome-gpu',
                    ('browser-chrome', 'screenshots') : 'browser-chrome-screenshots',
                    ('chrome', 'gpu') : 'chrome-gpu',
                    ('plain', 'gpu') : 'plain-gpu',
                    ('plain', 'media') : 'mochitest-media',
                    ('plain', 'webgl') : 'mochitest-gl',
                }
                if entry in subsuite_mapping:
                    suite = subsuite_mapping[entry]
                else:
                    suite = entry[0]
                suite_files = self.suites.get(suite)
                if not suite_files:
                    suite_files = []
                suite_files.append(file)
                self.suites[suite] = suite_files

    def _find_wpt_tests(self, dirs, changed_files):
        # Setup sys.path to include all the dependencies required to import
        # the web-platform-tests manifest parser. web-platform-tests provides
        # the localpaths.py to do the path manipulation, which we load,
        # providing the __file__ variable so it can resolve the relative
        # paths correctly.
        paths_file = os.path.join(dirs['abs_wpttest_dir'],
                                  "tests", "tools", "localpaths.py")
        execfile(paths_file, {"__file__": paths_file})
        import manifest as wptmanifest
        tests_root = os.path.join(dirs['abs_wpttest_dir'], "tests")
        man_path = os.path.join(dirs['abs_wpttest_dir'], "meta", "MANIFEST.json")
        man = wptmanifest.manifest.load(tests_root, man_path)

        repo_tests_path = os.path.join("testing", "web-platform", "tests")
        tests_path = os.path.join("tests", "web-platform", "tests")
        for (type, path, test) in man:
            if type not in ["testharness", "reftest", "wdspec"]:
                continue
            repo_path = os.path.join(repo_tests_path, path)
            # manifest paths use os.sep (like backslash on Windows) but
            # automation-relevance uses posixpath.sep
            repo_path = repo_path.replace(os.sep, posixpath.sep)
            if repo_path in changed_files:
                self.info("found web-platform test file '%s', type %s" % (path, type))
                suite_files = self.suites.get(type)
                if not suite_files:
                    suite_files = []
                path = os.path.join(tests_path, path)
                suite_files.append(path)
                self.suites[type] = suite_files

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
            return []

        def get_automationrelevance():
            response = self.load_json_url(url)
            return response

        dirs = self.query_abs_dirs()
        mozinfo.find_and_update_from_json(dirs['abs_test_install_dir'])
        e10s = self.config.get('e10s', False)
        mozinfo.update({"e10s": e10s})
        headless = self.config.get('headless', False)
        mozinfo.update({"headless": headless})
        # FIXME(emilio): Need to update test expectations.
        mozinfo.update({'stylo': True})
        mozinfo.update({'verify': True})
        self.info("Per-test run using mozinfo: %s" % str(mozinfo.info))

        # determine which files were changed on this push
        url = '%s/json-automationrelevance/%s' % (repository.rstrip('/'), revision)
        contents = self.retry(get_automationrelevance, attempts=2, sleeptime=10)
        changed_files = set()
        for c in contents['changesets']:
            self.info(" {cset} {desc}".format(
                cset=c['node'][0:12],
                desc=c['desc'].splitlines()[0].encode('ascii', 'ignore')))
            changed_files |= set(c['files'])

        if self.config.get('per_test_category') == "web-platform":
            self._find_wpt_tests(dirs, changed_files)
        else:
            self._find_misc_tests(dirs, changed_files)

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

        references = re.compile(r"(-ref|-notref|-noref|-noref.)\.")
        files = []
        jsreftest_extra_dir = os.path.join('js', 'src', 'tests')
        # For some suites, the test path needs to be updated before passing to
        # the test harness.
        for file in self.suites.get(suite):
            if (self.config.get('per_test_category') != "web-platform" and
                suite in ['reftest', 'crashtest']):
                file = os.path.join(self.reftest_test_dir, file)
                if suite == 'reftest':
                    # Special handling for modified reftest reference files:
                    #  - if both test and reference modified, run the test file
                    #  - if only reference modified, run the test file
                    nonref = references.sub('.', file)
                    if nonref != file:
                        file = None
                        if nonref not in files and os.path.exists(nonref):
                            file = nonref
            elif (self.config.get('per_test_category') != "web-platform" and
                  suite == 'jsreftest'):
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
            if self.config.get('per_test_category') == "web-platform":
                suites = self.suites.keys()
            elif all_suites and self.tests_downloaded:
                suites = dict((key, all_suites.get(key)) for key in
                    self.suites if key in all_suites.keys())
            else:
                # Until test zips are downloaded, manifests are not available,
                # so it is not possible to determine which suites are active/
                # required for per-test mode; assume all suites from supported
                # suite categories are required.
                if category in ['mochitest', 'xpcshell', 'reftest']:
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
            max_test_name_len = max_test_name_len - len('.../')
            while len(new) < max_test_name_len:
                head, tail = os.path.split(head)
                previous = new
                new = os.path.join(tail, new)
            test_name = os.path.join('...', previous or new)
            test_name = test_name.rstrip(os.path.sep)
        self.log("TinderboxPrint: Per-test run of %s<br/>: %s" %
                 (test_name, tbpl_status), level=log_level)

