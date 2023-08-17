#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import json
import os
import posixpath
import shutil
import sys
import tempfile
import uuid
import zipfile

import mozinfo

from mozharness.base.script import PostScriptAction, PreScriptAction
from mozharness.mozilla.testing.per_test_base import SingleTestMixin

code_coverage_config_options = [
    [
        ["--code-coverage"],
        {
            "action": "store_true",
            "dest": "code_coverage",
            "default": False,
            "help": "Whether gcov c++ code coverage should be run.",
        },
    ],
    [
        ["--per-test-coverage"],
        {
            "action": "store_true",
            "dest": "per_test_coverage",
            "default": False,
            "help": "Whether per-test coverage should be collected.",
        },
    ],
    [
        ["--disable-ccov-upload"],
        {
            "action": "store_true",
            "dest": "disable_ccov_upload",
            "default": False,
            "help": "Whether test run should package and upload code coverage data.",
        },
    ],
    [
        ["--java-code-coverage"],
        {
            "action": "store_true",
            "dest": "java_code_coverage",
            "default": False,
            "help": "Whether Java code coverage should be run.",
        },
    ],
]


class CodeCoverageMixin(SingleTestMixin):
    """
    Mixin for setting GCOV_PREFIX during test execution, packaging up
    the resulting .gcda files and uploading them to blobber.
    """

    gcov_dir = None
    grcov_dir = None
    grcov_bin = None
    jsvm_dir = None
    prefix = None
    per_test_reports = {}

    def __init__(self, **kwargs):
        if mozinfo.os == "linux" or mozinfo.os == "mac":
            self.grcov_bin = "grcov"
        elif mozinfo.os == "win":
            self.grcov_bin = "grcov.exe"
        else:
            raise Exception("Unexpected OS: {}".format(mozinfo.os))

        super(CodeCoverageMixin, self).__init__(**kwargs)

    @property
    def code_coverage_enabled(self):
        try:
            return bool(self.config.get("code_coverage"))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def per_test_coverage(self):
        try:
            return bool(self.config.get("per_test_coverage"))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def ccov_upload_disabled(self):
        try:
            return bool(self.config.get("disable_ccov_upload"))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def jsd_code_coverage_enabled(self):
        try:
            return bool(self.config.get("jsd_code_coverage"))
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def java_code_coverage_enabled(self):
        try:
            return bool(self.config.get("java_code_coverage"))
        except (AttributeError, KeyError, TypeError):
            return False

    def _setup_cpp_js_coverage_tools(self):
        fetches_dir = os.environ["MOZ_FETCHES_DIR"]
        with open(os.path.join(fetches_dir, "target.mozinfo.json"), "r") as f:
            build_mozinfo = json.load(f)

        self.prefix = build_mozinfo["topsrcdir"]

        strip_count = len(list(filter(None, self.prefix.split("/"))))
        os.environ["GCOV_PREFIX_STRIP"] = str(strip_count)

        # Download the gcno archive from the build machine.
        url_to_gcno = self.query_build_dir_url("target.code-coverage-gcno.zip")
        self.download_file(url_to_gcno, parent_dir=self.grcov_dir)

        # Download the chrome-map.json file from the build machine.
        url_to_chrome_map = self.query_build_dir_url("chrome-map.json")
        self.download_file(url_to_chrome_map, parent_dir=self.grcov_dir)

    def _setup_java_coverage_tools(self):
        # Download and extract jacoco-cli from the build task.
        url_to_jacoco = self.query_build_dir_url("target.jacoco-cli.jar")
        self.jacoco_jar = os.path.join(tempfile.mkdtemp(), "target.jacoco-cli.jar")
        self.download_file(url_to_jacoco, self.jacoco_jar)

        # Download and extract class files from the build task.
        self.classfiles_dir = tempfile.mkdtemp()
        for archive in ["target.geckoview_classfiles.zip", "target.app_classfiles.zip"]:
            url_to_classfiles = self.query_build_dir_url(archive)
            classfiles_zip_path = os.path.join(self.classfiles_dir, archive)
            self.download_file(url_to_classfiles, classfiles_zip_path)
            with zipfile.ZipFile(classfiles_zip_path, "r") as z:
                z.extractall(self.classfiles_dir)
            os.remove(classfiles_zip_path)

        # Create the directory where the emulator coverage file will be placed.
        self.java_coverage_output_dir = tempfile.mkdtemp()

    @PostScriptAction("download-and-extract")
    def setup_coverage_tools(self, action, success=None):
        if not self.code_coverage_enabled and not self.java_code_coverage_enabled:
            return

        self.grcov_dir = os.path.join(os.environ["MOZ_FETCHES_DIR"], "grcov")
        if not os.path.isfile(os.path.join(self.grcov_dir, self.grcov_bin)):
            raise Exception(
                "File not found: {}".format(
                    os.path.join(self.grcov_dir, self.grcov_bin)
                )
            )

        if self.code_coverage_enabled:
            self._setup_cpp_js_coverage_tools()

        if self.java_code_coverage_enabled:
            self._setup_java_coverage_tools()

    @PostScriptAction("download-and-extract")
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
        baseline_tests_by_ext = {
            ".html": {
                "test": "testing/mochitest/baselinecoverage/plain/test_baselinecoverage.html",
                "suite": "mochitest-plain",
            },
            ".js": {
                "test": "testing/mochitest/baselinecoverage/browser_chrome/browser_baselinecoverage.js",  # NOQA: E501
                "suite": "mochitest-browser-chrome",
            },
            ".xhtml": {
                "test": "testing/mochitest/baselinecoverage/chrome/test_baselinecoverage.xhtml",
                "suite": "mochitest-chrome",
            },
        }

        baseline_tests_by_suite = {
            "mochitest-browser-chrome": "testing/mochitest/baselinecoverage/browser_chrome/"
            "browser_baselinecoverage_browser-chrome.js"
        }

        wpt_baseline_test = "tests/web-platform/mozilla/tests/baselinecoverage/wpt_baselinecoverage.html"  # NOQA: E501
        if self.config.get("per_test_category") == "web-platform":
            if "testharness" not in self.suites:
                self.suites["testharness"] = []
            if wpt_baseline_test not in self.suites["testharness"]:
                self.suites["testharness"].append(wpt_baseline_test)
            return

        # Go through all the tests and find all
        # the baseline tests that are needed.
        tests_to_add = {}
        for suite in self.suites:
            if len(self.suites[suite]) == 0:
                continue
            if suite in baseline_tests_by_suite:
                if suite not in tests_to_add:
                    tests_to_add[suite] = []
                tests_to_add[suite].append(baseline_tests_by_suite[suite])
                continue

            # Default to file types if the suite has no baseline
            for test in self.suites[suite]:
                _, test_ext = os.path.splitext(test)

                if test_ext not in baseline_tests_by_ext:
                    # Add the '.js' test as a default baseline
                    # if none other exists.
                    test_ext = ".js"
                baseline_test_suite = baseline_tests_by_ext[test_ext]["suite"]
                baseline_test_name = baseline_tests_by_ext[test_ext]["test"]

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

    def set_coverage_env(self, env, is_baseline_test=False):
        # Set the GCOV directory.
        self.gcov_dir = tempfile.mkdtemp()
        env["GCOV_PREFIX"] = self.gcov_dir

        # Set the GCOV/JSVM directories where counters will be dumped in per-test mode.
        if self.per_test_coverage and not is_baseline_test:
            env["GCOV_RESULTS_DIR"] = tempfile.mkdtemp()
            env["JSVM_RESULTS_DIR"] = tempfile.mkdtemp()

        # Set JSVM directory.
        self.jsvm_dir = tempfile.mkdtemp()
        env["JS_CODE_COVERAGE_OUTPUT_DIR"] = self.jsvm_dir

    @PreScriptAction("run-tests")
    def _set_gcov_prefix(self, action):
        if not self.code_coverage_enabled:
            return

        if self.per_test_coverage:
            return

        self.set_coverage_env(os.environ)

    def parse_coverage_artifacts(
        self,
        gcov_dir,
        jsvm_dir,
        merge=False,
        output_format="lcov",
        filter_covered=False,
    ):
        jsvm_output_file = "jsvm_lcov_output.info"
        grcov_output_file = "grcov_lcov_output.info"

        dirs = self.query_abs_dirs()

        sys.path.append(dirs["abs_test_install_dir"])
        sys.path.append(os.path.join(dirs["abs_test_install_dir"], "mozbuild"))

        from codecoverage.lcov_rewriter import LcovFileRewriter

        jsvm_files = [os.path.join(jsvm_dir, e) for e in os.listdir(jsvm_dir)]
        appdir = self.config.get("appdir", "dist/bin/browser/")
        rewriter = LcovFileRewriter(
            os.path.join(self.grcov_dir, "chrome-map.json"), appdir
        )
        rewriter.rewrite_files(jsvm_files, jsvm_output_file, "")

        # Run grcov on the zipped .gcno and .gcda files.
        grcov_command = [
            os.path.join(self.grcov_dir, self.grcov_bin),
            "-t",
            output_format,
            "-p",
            self.prefix,
            "--ignore",
            "**/fetches/*",
            os.path.join(self.grcov_dir, "target.code-coverage-gcno.zip"),
            gcov_dir,
        ]

        if "coveralls" in output_format:
            grcov_command += ["--token", "UNUSED", "--commit-sha", "UNUSED"]

        if merge:
            grcov_command += [jsvm_output_file]

        if mozinfo.os == "win" or mozinfo.os == "mac":
            grcov_command += ["--llvm"]

        if filter_covered:
            grcov_command += ["--filter", "covered"]

        def skip_cannot_normalize(output_to_filter):
            return "\n".join(
                line
                for line in output_to_filter.rstrip().splitlines()
                if "cannot be normalized because" not in line
            )

        # 'grcov_output' will be a tuple, the first variable is the path to the lcov output,
        # the other is the path to the standard error output.
        tmp_output_file, _ = self.get_output_from_command(
            grcov_command,
            silent=True,
            save_tmpfiles=True,
            return_type="files",
            throw_exception=True,
            output_filter=skip_cannot_normalize,
        )
        shutil.move(tmp_output_file, grcov_output_file)

        shutil.rmtree(gcov_dir)
        shutil.rmtree(jsvm_dir)

        if merge:
            os.remove(jsvm_output_file)
            return grcov_output_file
        else:
            return grcov_output_file, jsvm_output_file

    def add_per_test_coverage_report(self, env, suite, test):
        gcov_dir = (
            env["GCOV_RESULTS_DIR"] if "GCOV_RESULTS_DIR" in env else self.gcov_dir
        )
        jsvm_dir = (
            env["JSVM_RESULTS_DIR"] if "JSVM_RESULTS_DIR" in env else self.jsvm_dir
        )

        grcov_file = self.parse_coverage_artifacts(
            gcov_dir,
            jsvm_dir,
            merge=True,
            output_format="coveralls",
            filter_covered=True,
        )

        report_file = str(uuid.uuid4()) + ".json"
        shutil.move(grcov_file, report_file)

        # Get the test path relative to topsrcdir.
        # This mapping is constructed by self.find_modified_tests().
        test = self.test_src_path.get(test.replace(os.sep, posixpath.sep), test)

        # Log a warning if the test path is still an absolute path.
        if os.path.isabs(test):
            self.warn("Found absolute path for test: {}".format(test))

        if suite not in self.per_test_reports:
            self.per_test_reports[suite] = {}
        assert test not in self.per_test_reports[suite]
        self.per_test_reports[suite][test] = report_file

        if "GCOV_RESULTS_DIR" in env:
            assert "JSVM_RESULTS_DIR" in env
            # In this case, parse_coverage_artifacts has removed GCOV_RESULTS_DIR and
            # JSVM_RESULTS_DIR so we need to remove GCOV_PREFIX and JS_CODE_COVERAGE_OUTPUT_DIR.
            try:
                shutil.rmtree(self.gcov_dir)
            except FileNotFoundError:
                pass

            try:
                shutil.rmtree(self.jsvm_dir)
            except FileNotFoundError:
                pass

    def is_covered(self, sf):
        # For C/C++ source files, we can consider a file as being uncovered
        # when all its source lines are uncovered.
        all_lines_uncovered = all(c is None or c == 0 for c in sf["coverage"])
        if all_lines_uncovered:
            return False

        # For JavaScript files, we can't do the same, as the top-level is always
        # executed, even if it just contains declarations. So, we need to check if
        # all its functions, except the top-level, are uncovered.
        functions = sf["functions"] if "functions" in sf else []
        all_functions_uncovered = all(
            not f["exec"] or f["name"] == "top-level" for f in functions
        )
        if all_functions_uncovered and len(functions) > 1:
            return False

        return True

    @PostScriptAction("run-tests")
    def _package_coverage_data(self, action, success=None):
        dirs = self.query_abs_dirs()

        if not self.code_coverage_enabled:
            return

        if self.per_test_coverage:
            if not self.per_test_reports:
                self.info("No tests were found...not saving coverage data.")
                return

            # Get the baseline tests that were run.
            baseline_tests_ext_cov = {}
            baseline_tests_suite_cov = {}
            for suite, data in self.per_test_reports.items():
                for test, grcov_file in data.items():
                    if "baselinecoverage" not in test:
                        continue

                    # TODO: Optimize this part which loads JSONs
                    # with a size of about 40Mb into memory for diffing later.
                    # Bug 1460064 is filed for this.
                    with open(grcov_file, "r") as f:
                        data = json.load(f)

                    if suite in os.path.split(test)[-1]:
                        baseline_tests_suite_cov[suite] = data
                    else:
                        _, baseline_filetype = os.path.splitext(test)
                        baseline_tests_ext_cov[baseline_filetype] = data

            dest = os.path.join(
                dirs["abs_blob_upload_dir"], "per-test-coverage-reports.zip"
            )
            with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
                for suite, data in self.per_test_reports.items():
                    for test, grcov_file in data.items():
                        if "baselinecoverage" in test:
                            # Don't keep the baseline coverage
                            continue
                        else:
                            # Get test coverage
                            with open(grcov_file, "r") as f:
                                report = json.load(f)

                            # Remove uncovered files, as they are unneeded for per-test
                            # coverage purposes.
                            report["source_files"] = [
                                sf
                                for sf in report["source_files"]
                                if self.is_covered(sf)
                            ]

                            # Get baseline coverage
                            baseline_coverage = {}
                            if suite in baseline_tests_suite_cov:
                                baseline_coverage = baseline_tests_suite_cov[suite]
                            elif self.config.get("per_test_category") == "web-platform":
                                baseline_coverage = baseline_tests_ext_cov[".html"]
                            else:
                                for file_type in baseline_tests_ext_cov:
                                    if not test.endswith(file_type):
                                        continue
                                    baseline_coverage = baseline_tests_ext_cov[
                                        file_type
                                    ]
                                    break

                            if not baseline_coverage:
                                # Default to the '.js' baseline as it is the largest
                                self.info("Did not find a baseline test for: " + test)
                                baseline_coverage = baseline_tests_ext_cov[".js"]

                            unique_coverage = rm_baseline_cov(baseline_coverage, report)

                        with open(grcov_file, "w") as f:
                            json.dump(
                                {
                                    "test": test,
                                    "suite": suite,
                                    "report": unique_coverage,
                                },
                                f,
                            )

                        z.write(grcov_file)
            return

        del os.environ["GCOV_PREFIX_STRIP"]
        del os.environ["GCOV_PREFIX"]
        del os.environ["JS_CODE_COVERAGE_OUTPUT_DIR"]

        if not self.ccov_upload_disabled:
            grcov_output_file, jsvm_output_file = self.parse_coverage_artifacts(
                self.gcov_dir, self.jsvm_dir
            )

            try:
                os.makedirs(dirs["abs_blob_upload_dir"])
            except OSError as e:
                if e.errno != errno.EEXIST:
                    raise

            # Zip the grcov output and upload it.
            grcov_zip_path = os.path.join(
                dirs["abs_blob_upload_dir"], "code-coverage-grcov.zip"
            )
            with zipfile.ZipFile(grcov_zip_path, "w", zipfile.ZIP_DEFLATED) as z:
                z.write(grcov_output_file)

            # Zip the JSVM coverage data and upload it.
            jsvm_zip_path = os.path.join(
                dirs["abs_blob_upload_dir"], "code-coverage-jsvm.zip"
            )
            with zipfile.ZipFile(jsvm_zip_path, "w", zipfile.ZIP_DEFLATED) as z:
                z.write(jsvm_output_file)

        shutil.rmtree(self.grcov_dir)

    @PostScriptAction("run-tests")
    def process_java_coverage_data(self, action, success=None):
        """
        Run JaCoCo on the coverage.ec file in order to get a XML report.
        After that, run grcov on the XML report to get a lcov report.
        Finally, archive the lcov file and upload it, as process_coverage_data is doing.
        """
        if not self.java_code_coverage_enabled:
            return

        # If the emulator became unresponsive, the task has failed and we don't
        # have any coverage report file, so stop running this function and
        # allow the task to be retried automatically.
        if not success and not os.listdir(self.java_coverage_output_dir):
            return

        report_files = [
            os.path.join(self.java_coverage_output_dir, f)
            for f in os.listdir(self.java_coverage_output_dir)
        ]
        assert len(report_files) > 0, "JaCoCo coverage data files were not found."

        dirs = self.query_abs_dirs()
        xml_path = tempfile.mkdtemp()
        jacoco_command = (
            ["java", "-jar", self.jacoco_jar, "report"]
            + report_files
            + [
                "--classfiles",
                self.classfiles_dir,
                "--name",
                "geckoview-junit",
                "--xml",
                os.path.join(xml_path, "geckoview-junit.xml"),
            ]
        )
        self.run_command(jacoco_command, halt_on_failure=True)

        grcov_command = [
            os.path.join(self.grcov_dir, self.grcov_bin),
            "-t",
            "lcov",
            xml_path,
        ]
        tmp_output_file, _ = self.get_output_from_command(
            grcov_command,
            silent=True,
            save_tmpfiles=True,
            return_type="files",
            throw_exception=True,
        )

        if not self.ccov_upload_disabled:
            grcov_zip_path = os.path.join(
                dirs["abs_blob_upload_dir"], "code-coverage-grcov.zip"
            )
            with zipfile.ZipFile(grcov_zip_path, "w", zipfile.ZIP_DEFLATED) as z:
                z.write(tmp_output_file, "grcov_lcov_output.info")


def rm_baseline_cov(baseline_coverage, test_coverage):
    """
    Returns the difference between test_coverage and
    baseline_coverage, such that what is returned
    is the unique coverage for the test in question.
    """

    # Get all files into a quicker search format
    unique_test_coverage = test_coverage
    baseline_files = {el["name"]: el for el in baseline_coverage["source_files"]}
    test_files = {el["name"]: el for el in test_coverage["source_files"]}

    # Perform the difference and find everything
    # unique to the test.
    unique_file_coverage = {}
    for test_file in test_files:
        if test_file not in baseline_files:
            unique_file_coverage[test_file] = test_files[test_file]
            continue

        if len(test_files[test_file]["coverage"]) != len(
            baseline_files[test_file]["coverage"]
        ):
            # File has line number differences due to gcov bug:
            #  https://bugzilla.mozilla.org/show_bug.cgi?id=1410217
            continue

        # TODO: Attempt to rewrite this section to remove one of the two
        # iterations over a test's source file's coverage for optimization.
        # Bug 1460064 was filed for this.

        # Get line numbers and the differences
        file_coverage = {
            i
            for i, cov in enumerate(test_files[test_file]["coverage"])
            if cov is not None and cov > 0
        }

        baseline = {
            i
            for i, cov in enumerate(baseline_files[test_file]["coverage"])
            if cov is not None and cov > 0
        }

        unique_coverage = file_coverage - baseline

        if len(unique_coverage) > 0:
            unique_file_coverage[test_file] = test_files[test_file]

            # Return the data to original format to return
            # coverage within the test_coverge data object.
            fmt_unique_coverage = []
            for i, cov in enumerate(unique_file_coverage[test_file]["coverage"]):
                if cov is None:
                    fmt_unique_coverage.append(None)
                    continue

                # TODO: Bug 1460061, determine if hit counts
                # need to be considered.
                if cov > 0:
                    # If there is a count
                    if i in unique_coverage:
                        # Only add the count if it's unique
                        fmt_unique_coverage.append(
                            unique_file_coverage[test_file]["coverage"][i]
                        )
                        continue
                # Zero out everything that is not unique
                fmt_unique_coverage.append(0)
            unique_file_coverage[test_file]["coverage"] = fmt_unique_coverage

    # Reformat to original test_coverage list structure
    unique_test_coverage["source_files"] = list(unique_file_coverage.values())

    return unique_test_coverage
