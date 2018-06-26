#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import shutil
import sys
import tarfile
import tempfile
import zipfile
import uuid

import mozinfo
from mozharness.base.script import (
    PreScriptAction,
    PostScriptAction,
)
from mozharness.mozilla.testing.per_test_base import SingleTestMixin


code_coverage_config_options = [
    [["--code-coverage"],
     {"action": "store_true",
      "dest": "code_coverage",
      "default": False,
      "help": "Whether gcov c++ code coverage should be run."
      }],
    [["--per-test-coverage"],
     {"action": "store_true",
      "dest": "per_test_coverage",
      "default": False,
      "help": "Whether per-test coverage should be collected."
      }],
    [["--disable-ccov-upload"],
     {"action": "store_true",
      "dest": "disable_ccov_upload",
      "default": False,
      "help": "Whether test run should package and upload code coverage data."
      }],
    [["--jsd-code-coverage"],
     {"action": "store_true",
      "dest": "jsd_code_coverage",
      "default": False,
      "help": "Whether JSDebugger code coverage should be run."
      }],
]


class CodeCoverageMixin(SingleTestMixin):
    """
    Mixin for setting GCOV_PREFIX during test execution, packaging up
    the resulting .gcda files and uploading them to blobber.
    """
    gcov_dir = None
    jsvm_dir = None
    prefix = None
    per_test_reports = {}

    def __init__(self):
        super(CodeCoverageMixin, self).__init__()

    @property
    def code_coverage_enabled(self):
        try:
            return bool(self.config.get('code_coverage'))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def per_test_coverage(self):
        try:
            return bool(self.config.get('per_test_coverage'))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def ccov_upload_disabled(self):
        try:
            return bool(self.config.get('disable_ccov_upload'))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def jsd_code_coverage_enabled(self):
        try:
            return bool(self.config.get('jsd_code_coverage'))
        except (AttributeError, KeyError, TypeError):
            return False

    @PostScriptAction('download-and-extract')
    def setup_coverage_tools(self, action, success=None):
        if not self.code_coverage_enabled:
            return

        if mozinfo.os == 'linux' or mozinfo.os == 'mac':
            self.prefix = '/builds/worker/workspace/build/src/'
            strip_count = self.prefix.count('/')
        elif mozinfo.os == 'win':
            self.prefix = 'z:/build/build/src/'
            # Add 1 as on Windows the path where the compiler tries to write the
            # gcda files has an additional 'obj-firefox' component.
            strip_count = self.prefix.count('/') + 1
        else:
            raise Exception('Unexpected OS: {}'.format(mozinfo.os))

        os.environ['GCOV_PREFIX_STRIP'] = str(strip_count)

        # Install grcov on the test machine
        # Get the path to the build machines gcno files.
        self.url_to_gcno = self.query_build_dir_url('target.code-coverage-gcno.zip')
        self.url_to_chrome_map = self.query_build_dir_url('chrome-map.json')
        dirs = self.query_abs_dirs()

        # Create the grcov directory, get the tooltool manifest, and finally
        # download and unpack the grcov binary.
        self.grcov_dir = tempfile.mkdtemp()

        if mozinfo.os == 'linux':
            platform = 'linux64'
            tar_file = 'grcov-linux-x86_64.tar.bz2'
        elif mozinfo.os == 'win':
            platform = 'win32'
            tar_file = 'grcov-win-i686.tar.bz2'
        elif mozinfo.os == 'mac':
            platform = 'macosx64'
            tar_file = 'grcov-osx-x86_64.tar.bz2'

        manifest = os.path.join(dirs.get('abs_test_install_dir',
                                         os.path.join(dirs['abs_work_dir'], 'tests')),
                                'config/tooltool-manifests/%s/ccov.manifest' % platform)

        self.tooltool_fetch(
            manifest=manifest,
            output_dir=self.grcov_dir,
            cache=self.config.get('tooltool_cache')
        )

        with tarfile.open(os.path.join(self.grcov_dir, tar_file)) as tar:
            tar.extractall(self.grcov_dir)

        # Download the gcno archive from the build machine.
        self.download_file(self.url_to_gcno, parent_dir=self.grcov_dir)

        # Download the chrome-map.json file from the build machine.
        self.download_file(self.url_to_chrome_map, parent_dir=self.grcov_dir)

    @PostScriptAction('download-and-extract')
    def find_tests_for_coverage(self, action, success=None):
        """
           For each file modified on this push, determine if the modified file
           is a test, by searching test manifests. Populate self.verify_suites
           with test files, organized by suite.

           This depends on test manifests, so can only run after test zips have
           been downloaded and extracted.
        """
        if not self.per_test_coverage:
            return

        self.find_modified_tests()

        # TODO: Add tests that haven't been run for a while (a week? N pushes?)

        # Add baseline code coverage collection tests
        baseline_tests = {
            '.html': {
                'test': 'testing/mochitest/baselinecoverage/plain/test_baselinecoverage.html',
                'suite': 'plain'
            },
            '.js': {
                'test': 'testing/mochitest/baselinecoverage/browser_chrome/browser_baselinecoverage.js',  # NOQA: E501
                'suite': 'browser-chrome'
            },
            '.xul': {
                'test': 'testing/mochitest/baselinecoverage/chrome/test_baselinecoverage.xul',
                'suite': 'chrome'
            }
        }

        wpt_baseline_test = 'tests/web-platform/mozilla/tests/baselinecoverage/wpt_baselinecoverage.html'  # NOQA: E501
        if self.config.get('per_test_category') == "web-platform":
            if 'testharness' not in self.suites:
                self.suites['testharness'] = []
            if wpt_baseline_test not in self.suites['testharness']:
                self.suites["testharness"].append(wpt_baseline_test)
            return

        # Go through all the tests and find all
        # the baseline tests that are needed.
        tests_to_add = {}
        for suite in self.suites:
            for test in self.suites[suite]:
                _, test_ext = os.path.splitext(test)

                if test_ext not in baseline_tests:
                    # Add the '.js' test as a default baseline
                    # if none other exists.
                    test_ext = '.js'
                baseline_test_suite = baseline_tests[test_ext]['suite']
                baseline_test_name = baseline_tests[test_ext]['test']

                if baseline_test_suite not in tests_to_add:
                    tests_to_add[baseline_test_suite] = []
                if baseline_test_name not in tests_to_add[baseline_test_suite]:
                    tests_to_add[baseline_test_suite].append(baseline_test_name)

        # Add all baseline tests needed
        for suite in tests_to_add:
            for test in tests_to_add[suite]:
                if suite not in self.suites:
                    self.suites[suite] = []
                if test not in self.suites[suite]:
                    self.suites[suite].append(test)

    @property
    def coverage_args(self):
        return []

    def set_coverage_env(self, env):
        # Set the GCOV directory.
        gcov_dir = tempfile.mkdtemp()
        env['GCOV_PREFIX'] = gcov_dir

        # Set JSVM directory.
        jsvm_dir = tempfile.mkdtemp()
        env['JS_CODE_COVERAGE_OUTPUT_DIR'] = jsvm_dir

        return (gcov_dir, jsvm_dir)

    @PreScriptAction('run-tests')
    def _set_gcov_prefix(self, action):
        if not self.code_coverage_enabled:
            return

        if self.per_test_coverage:
            return

        self.gcov_dir, self.jsvm_dir = self.set_coverage_env(os.environ)

    def parse_coverage_artifacts(self,
                                 gcov_dir,
                                 jsvm_dir,
                                 merge=False,
                                 output_format='lcov',
                                 filter_covered=False):
        jsvm_output_file = 'jsvm_lcov_output.info'
        grcov_output_file = 'grcov_lcov_output.info'

        dirs = self.query_abs_dirs()

        sys.path.append(dirs['abs_test_install_dir'])
        sys.path.append(os.path.join(dirs['abs_test_install_dir'], 'mozbuild/codecoverage'))

        from lcov_rewriter import LcovFileRewriter
        jsvm_files = [os.path.join(jsvm_dir, e) for e in os.listdir(jsvm_dir)]
        rewriter = LcovFileRewriter(os.path.join(self.grcov_dir, 'chrome-map.json'))
        rewriter.rewrite_files(jsvm_files, jsvm_output_file, '')

        # Run grcov on the zipped .gcno and .gcda files.
        grcov_command = [
            os.path.join(self.grcov_dir, 'grcov'),
            '-t', output_format,
            '-p', self.prefix,
            '--ignore-dir', 'gcc*',
            '--ignore-dir', 'vs2017_*',
            os.path.join(self.grcov_dir, 'target.code-coverage-gcno.zip'), gcov_dir
        ]

        if 'coveralls' in output_format:
            grcov_command += ['--token', 'UNUSED', '--commit-sha', 'UNUSED']

        if merge:
            grcov_command += [jsvm_output_file]

        if mozinfo.os == 'win' or mozinfo.os == 'mac':
            grcov_command += ['--llvm']

        if filter_covered:
            grcov_command += ['--filter', 'covered']

        # 'grcov_output' will be a tuple, the first variable is the path to the lcov output,
        # the other is the path to the standard error output.
        tmp_output_file, _ = self.get_output_from_command(
            grcov_command,
            silent=True,
            save_tmpfiles=True,
            return_type='files',
            throw_exception=True,
        )
        shutil.move(tmp_output_file, grcov_output_file)

        shutil.rmtree(gcov_dir)
        shutil.rmtree(jsvm_dir)

        if merge:
            os.remove(jsvm_output_file)
            return grcov_output_file
        else:
            return grcov_output_file, jsvm_output_file

    def add_per_test_coverage_report(self, gcov_dir, jsvm_dir, suite, test):
        grcov_file = self.parse_coverage_artifacts(
            gcov_dir, jsvm_dir, merge=True, output_format='coveralls',
            filter_covered=True,
        )

        report_file = str(uuid.uuid4()) + '.json'
        shutil.move(grcov_file, report_file)

        if suite not in self.per_test_reports:
            self.per_test_reports[suite] = {}
        assert test not in self.per_test_reports[suite]
        self.per_test_reports[suite][test] = report_file

    def is_covered(self, sf):
        # For C/C++ source files, we can consider a file as being uncovered
        # when all its source lines are uncovered.
        all_lines_uncovered = all(c is None or c == 0 for c in sf['coverage'])
        if all_lines_uncovered:
            return False

        # For JavaScript files, we can't do the same, as the top-level is always
        # executed, even if it just contains declarations. So, we need to check if
        # all its functions, except the top-level, are uncovered.
        functions = sf['functions'] if 'functions' in sf else []
        all_functions_uncovered = all(not f['exec'] or f['name'] == 'top-level' for f in functions)
        if all_functions_uncovered and len(functions) > 1:
            return False

        return True

    @PostScriptAction('run-tests')
    def _package_coverage_data(self, action, success=None):
        dirs = self.query_abs_dirs()

        if self.jsd_code_coverage_enabled:
            # Setup the command for compression
            jsdcov_dir = dirs['abs_blob_upload_dir']
            zipFile = os.path.join(jsdcov_dir, "jsdcov_artifacts.zip")

            self.info("Beginning compression of JSDCov artifacts...")
            with zipfile.ZipFile(zipFile, 'w', zipfile.ZIP_DEFLATED) as z:
                for filename in os.listdir(jsdcov_dir):
                    if filename.startswith("jscov") and filename.endswith(".json"):
                        path = os.path.join(jsdcov_dir, filename)
                        z.write(path, filename)
                        # Delete already compressed JSCov artifacts.
                        os.remove(path)

            self.info("Completed compression of JSDCov artifacts!")
            self.info("Path to JSDCov compressed artifacts: " + zipFile)

        if not self.code_coverage_enabled:
            return

        if self.per_test_coverage:
            if not self.per_test_reports:
                self.info("No tests were found...not saving coverage data.")
                return

            # Get the baseline tests that were run.
            baseline_tests_cov = {}
            for suite, data in self.per_test_reports.items():
                for test, grcov_file in data.items():
                    if 'baselinecoverage' not in test:
                        continue

                    # TODO: Optimize this part which loads JSONs
                    # with a size of about 40Mb into memory for diffing later.
                    # Bug 1460064 is filed for this.
                    _, baseline_filetype = os.path.splitext(test)
                    with open(grcov_file, 'r') as f:
                        baseline_tests_cov[baseline_filetype] = json.load(f)

            dest = os.path.join(dirs['abs_blob_upload_dir'], 'per-test-coverage-reports.zip')
            with zipfile.ZipFile(dest, 'w', zipfile.ZIP_DEFLATED) as z:
                for suite, data in self.per_test_reports.items():
                    for test, grcov_file in data.items():
                        if 'baselinecoverage' in test:
                            # Don't keep the baseline coverage
                            continue
                        else:
                            # Get test coverage
                            with open(grcov_file, 'r') as f:
                                report = json.load(f)

                            # Remove uncovered files, as they are unneeded for per-test
                            # coverage purposes.
                            report['source_files'] = [
                                sf for sf in report['source_files'] if self.is_covered(sf)]

                            # Get baseline coverage
                            baseline_coverage = {}
                            if self.config.get('per_test_category') == "web-platform":
                                baseline_coverage = baseline_tests_cov['.html']
                            else:
                                for file_type in baseline_tests_cov:
                                    if not test.endswith(file_type):
                                        continue
                                    baseline_coverage = baseline_tests_cov[file_type]
                                    break

                            if not baseline_coverage:
                                # Default to the '.js' baseline as it is the largest
                                self.info("Did not find a baseline test for: " + test)
                                baseline_coverage = baseline_tests_cov['.js']

                            unique_coverage = rm_baseline_cov(baseline_coverage, report)

                        with open(grcov_file, 'w') as f:
                            json.dump({
                                'test': test,
                                'suite': suite,
                                'report': unique_coverage,
                            }, f)

                        z.write(grcov_file)
            return

        del os.environ['GCOV_PREFIX_STRIP']
        del os.environ['GCOV_PREFIX']
        del os.environ['JS_CODE_COVERAGE_OUTPUT_DIR']

        if not self.ccov_upload_disabled:
            grcov_output_file, jsvm_output_file = self.parse_coverage_artifacts(self.gcov_dir,
                                                                                self.jsvm_dir)

            # Zip the grcov output and upload it.
            grcov_zip_path = os.path.join(dirs['abs_blob_upload_dir'], 'code-coverage-grcov.zip')
            with zipfile.ZipFile(grcov_zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
                z.write(grcov_output_file)

            # Zip the JSVM coverage data and upload it.
            jsvm_zip_path = os.path.join(dirs['abs_blob_upload_dir'], 'code-coverage-jsvm.zip')
            with zipfile.ZipFile(jsvm_zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
                z.write(jsvm_output_file)

        shutil.rmtree(self.grcov_dir)


def rm_baseline_cov(baseline_coverage, test_coverage):
    '''
    Returns the difference between test_coverage and
    baseline_coverage, such that what is returned
    is the unique coverage for the test in question.
    '''

    # Get all files into a quicker search format
    unique_test_coverage = test_coverage
    baseline_files = {el['name']: el for el in baseline_coverage['source_files']}
    test_files = {el['name']: el for el in test_coverage['source_files']}

    # Perform the difference and find everything
    # unique to the test.
    unique_file_coverage = {}
    for test_file in test_files:
        if test_file not in baseline_files:
            unique_file_coverage[test_file] = test_files[test_file]
            continue

        if len(test_files[test_file]['coverage']) != len(baseline_files[test_file]['coverage']):
            # File has line number differences due to gcov bug:
            #  https://bugzilla.mozilla.org/show_bug.cgi?id=1410217
            continue

        # TODO: Attempt to rewrite this section to remove one of the two
        # iterations over a test's source file's coverage for optimization.
        # Bug 1460064 was filed for this.

        # Get line numbers and the differences
        file_coverage = {i for i, cov in enumerate(test_files[test_file]['coverage'])
                         if cov is not None and cov > 0}

        baseline = {i for i, cov in enumerate(baseline_files[test_file]['coverage'])
                    if cov is not None and cov > 0}

        unique_coverage = file_coverage - baseline

        if len(unique_coverage) > 0:
            unique_file_coverage[test_file] = test_files[test_file]

            # Return the data to original format to return
            # coverage within the test_coverge data object.
            fmt_unique_coverage = []
            for i, cov in enumerate(unique_file_coverage[test_file]['coverage']):
                if cov is None:
                    fmt_unique_coverage.append(None)
                    continue

                # TODO: Bug 1460061, determine if hit counts
                # need to be considered.
                if cov > 0:
                    # If there is a count
                    if i in unique_coverage:
                        # Only add the count if it's unique
                        fmt_unique_coverage.append(unique_file_coverage[test_file]['coverage'][i])
                        continue
                # Zero out everything that is not unique
                fmt_unique_coverage.append(0)
            unique_file_coverage[test_file]['coverage'] = fmt_unique_coverage

    # Reformat to original test_coverage list structure
    unique_test_coverage['source_files'] = list(unique_file_coverage.values())

    return unique_test_coverage
