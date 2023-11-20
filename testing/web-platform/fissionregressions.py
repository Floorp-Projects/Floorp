# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import os
import re
import sys

from mozlog import commandline

run_infos = {
    "linux-opt": {
        "os": "linux",
        "processor": "x86_64",
        "version": "Ubuntu 18.04",
        "os_version": "18.04",
        "bits": 64,
        "has_sandbox": True,
        "automation": True,
        "linux_distro": "Ubuntu",
        "apple_silicon": False,
        "appname": "firefox",
        "artifact": False,
        "asan": False,
        "bin_suffix": "",
        "buildapp": "browser",
        "buildtype_guess": "pgo",
        "cc_type": "clang",
        "ccov": False,
        "crashreporter": True,
        "datareporting": True,
        "debug": False,
        "devedition": False,
        "early_beta_or_earlier": True,
        "healthreport": True,
        "nightly_build": True,
        "normandy": True,
        "official": True,
        "pgo": True,
        "platform_guess": "linux64",
        "release_or_beta": False,
        "require_signing": False,
        "stylo": True,
        "sync": True,
        "telemetry": False,
        "tests_enabled": True,
        "toolkit": "gtk",
        "tsan": False,
        "ubsan": False,
        "updater": True,
        "python_version": 3,
        "product": "firefox",
        "verify": False,
        "wasm": True,
        "e10s": True,
        "headless": False,
        "fission": True,
        "sessionHistoryInParent": True,
        "swgl": False,
        "privateBrowsing": False,
        "win10_2004": False,
        "win11_2009": False,
        "domstreams": True,
        "isolated_process": False,
        "display": "x11",
    },
    "linux-debug": {
        "os": "linux",
        "processor": "x86_64",
        "version": "Ubuntu 18.04",
        "os_version": "18.04",
        "bits": 64,
        "has_sandbox": True,
        "automation": True,
        "linux_distro": "Ubuntu",
        "apple_silicon": False,
        "appname": "firefox",
        "artifact": False,
        "asan": False,
        "bin_suffix": "",
        "buildapp": "browser",
        "buildtype_guess": "debug",
        "cc_type": "clang",
        "ccov": False,
        "crashreporter": True,
        "datareporting": True,
        "debug": True,
        "devedition": False,
        "early_beta_or_earlier": True,
        "healthreport": True,
        "nightly_build": True,
        "normandy": True,
        "official": True,
        "pgo": False,
        "platform_guess": "linux64",
        "release_or_beta": False,
        "require_signing": False,
        "stylo": True,
        "sync": True,
        "telemetry": False,
        "tests_enabled": True,
        "toolkit": "gtk",
        "tsan": False,
        "ubsan": False,
        "updater": True,
        "python_version": 3,
        "product": "firefox",
        "verify": False,
        "wasm": True,
        "e10s": True,
        "headless": False,
        "fission": False,
        "sessionHistoryInParent": False,
        "swgl": False,
        "privateBrowsing": False,
        "win10_2004": False,
        "win11_2009": False,
        "domstreams": True,
        "isolated_process": False,
        "display": "x11",
    },
    "win-opt": {
        "os": "win",
        "processor": "x86_64",
        "version": "10.0.17134",
        "os_version": "10.0",
        "bits": 64,
        "has_sandbox": True,
        "automation": True,
        "apple_silicon": False,
        "appname": "firefox",
        "artifact": False,
        "asan": False,
        "bin_suffix": ".exe",
        "buildapp": "browser",
        "buildtype_guess": "pgo",
        "cc_type": "clang-cl",
        "ccov": False,
        "crashreporter": True,
        "datareporting": True,
        "debug": False,
        "devedition": False,
        "early_beta_or_earlier": True,
        "healthreport": True,
        "nightly_build": True,
        "normandy": True,
        "official": True,
        "pgo": True,
        "platform_guess": "win64",
        "release_or_beta": False,
        "require_signing": False,
        "stylo": True,
        "sync": True,
        "telemetry": False,
        "tests_enabled": True,
        "toolkit": "windows",
        "tsan": False,
        "ubsan": False,
        "updater": True,
        "python_version": 3,
        "product": "firefox",
        "verify": False,
        "wasm": True,
        "e10s": True,
        "headless": False,
        "fission": False,
        "sessionHistoryInParent": False,
        "swgl": False,
        "privateBrowsing": False,
        "win10_2004": False,
        "win11_2009": False,
        "domstreams": True,
        "isolated_process": False,
        "display": None,
    },
}


# RE that checks for anything containing a three+ digit number
maybe_bug_re = re.compile(r".*\d\d\d+")


def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--all-json", type=os.path.abspath, help="Path to write json output to"
    )
    parser.add_argument(
        "--untriaged",
        type=os.path.abspath,
        help="Path to write list of regressions with no associated bug",
    )
    parser.add_argument(
        "--platform",
        dest="platforms",
        action="append",
        choices=list(run_infos.keys()),
        help="Configurations to compute fission changes for",
    )
    commandline.add_logging_group(parser)
    return parser


def allowed_results(test, subtest=None):
    return test.expected(subtest), test.known_intermittent(subtest)


def is_worse(baseline_result, new_result):
    if new_result == baseline_result:
        return False

    if new_result in ("PASS", "OK"):
        return False

    if baseline_result in ("PASS", "OK"):
        return True

    # A crash -> not crash isn't a regression
    if baseline_result == "CRASH":
        return False

    return True


def is_regression(baseline_result, new_result):
    if baseline_result == new_result:
        return False

    baseline_expected, baseline_intermittent = baseline_result
    new_expected, new_intermittent = new_result

    baseline_all = {baseline_expected} | set(baseline_intermittent)
    new_all = {new_expected} | set(new_intermittent)

    if baseline_all == new_all:
        return False

    if not baseline_intermittent and not new_intermittent:
        return is_worse(baseline_expected, new_expected)

    # If it was intermittent and isn't now, check if the new result is
    # worse than any of the previous results so that [PASS, FAIL] -> FAIL
    # looks like a regression
    if baseline_intermittent and not new_intermittent:
        return any(is_worse(result, new_expected) for result in baseline_all)

    # If it was a perma and is now intermittent, check if any new result is
    # worse than the previous result.
    if not baseline_intermittent and new_intermittent:
        return any(is_worse(baseline_expected, result) for result in new_all)

    # If it was an intermittent and is still an intermittent
    # check if any new result not in the old results is worse than
    # any old result
    new_results = new_all - baseline_all
    return any(
        is_worse(baseline_result, new_result)
        for new_result in new_results
        for baseline_result in baseline_all
    )


def get_meta_prop(test, subtest, name):
    for meta in test.itermeta(subtest):
        try:
            value = meta.get(name)
        except KeyError:
            pass
        else:
            return value
    return None


def include_result(result):
    if result.disabled or result.regressions:
        return True

    if isinstance(result, TestResult):
        for subtest_result in result.subtest_results.values():
            if subtest_result.disabled or subtest_result.regressions:
                return True

    return False


class Result:
    def __init__(self):
        self.bugs = set()
        self.disabled = set()
        self.regressions = {}

    def add_regression(self, platform, baseline_results, fission_results):
        self.regressions[platform] = {
            "baseline": [baseline_results[0]] + baseline_results[1],
            "fission": [fission_results[0]] + fission_results[1],
        }

    def to_json(self):
        raise NotImplementedError

    def is_triaged(self):
        raise NotImplementedError


class TestResult(Result):
    def __init__(self):
        super().__init__()
        self.subtest_results = {}

    def add_subtest(self, name):
        self.subtest_results[name] = SubtestResult(self)

    def to_json(self):
        rv = {}
        include_subtests = {
            name: item.to_json()
            for name, item in self.subtest_results.items()
            if include_result(item)
        }
        if include_subtests:
            rv["subtest_results"] = include_subtests
        if self.regressions:
            rv["regressions"] = self.regressions
        if self.disabled:
            rv["disabled"] = list(self.disabled)
        if self.bugs:
            rv["bugs"] = list(self.bugs)
        return rv

    def is_triaged(self):
        return bool(self.bugs) or (
            not self.regressions
            and all(
                subtest_result.is_triaged()
                for subtest_result in self.subtest_results.values()
            )
        )


class SubtestResult(Result):
    def __init__(self, parent):
        super().__init__()
        self.parent = parent

    def to_json(self):
        rv = {}
        if self.regressions:
            rv["regressions"] = self.regressions
        if self.disabled:
            rv["disabled"] = list(self.disabled)
        bugs = self.bugs - self.parent.bugs
        if bugs:
            rv["bugs"] = list(bugs)
        return rv

    def is_triaged(self):
        return bool(not self.regressions or self.parent.bugs or self.bugs)


def run(logger, src_root, obj_root, **kwargs):
    commandline.setup_logging(
        logger, {key: value for key, value in kwargs.items() if key.startswith("log_")}
    )

    import manifestupdate

    sys.path.insert(
        0,
        os.path.abspath(os.path.join(os.path.dirname(__file__), "tests", "tools")),
    )
    from wptrunner import testloader, wpttest

    logger.info("Loading test manifest")
    test_manifests = manifestupdate.run(src_root, obj_root, logger)

    test_results = {}

    platforms = kwargs["platforms"]
    if platforms is None:
        platforms = run_infos.keys()

    for platform in platforms:
        platform_run_info = run_infos[platform]
        run_info_baseline = platform_run_info.copy()
        run_info_baseline["fission"] = False

        tests = {}

        for kind in ("baseline", "fission"):
            logger.info("Loading tests %s %s" % (platform, kind))
            run_info = platform_run_info.copy()
            run_info["fission"] = kind == "fission"

            subsuites = testloader.load_subsuites(logger, run_info, None, set())
            test_loader = testloader.TestLoader(
                test_manifests,
                wpttest.enabled_tests,
                run_info,
                subsuites=subsuites,
                manifest_filters=[],
            )
            tests[kind] = {
                test.id: test
                for _, _, test in test_loader.iter_tests(
                    run_info, test_loader.manifest_filters
                )
                if test._test_metadata is not None
            }

        for test_id, baseline_test in tests["baseline"].items():
            fission_test = tests["fission"][test_id]

            if test_id not in test_results:
                test_results[test_id] = TestResult()

            test_result = test_results[test_id]

            baseline_bug = get_meta_prop(baseline_test, None, "bug")
            fission_bug = get_meta_prop(fission_test, None, "bug")
            if fission_bug and fission_bug != baseline_bug:
                test_result.bugs.add(fission_bug)

            if fission_test.disabled() and not baseline_test.disabled():
                test_result.disabled.add(platform)
                reason = get_meta_prop(fission_test, None, "disabled")
                if reason and maybe_bug_re.match(reason):
                    test_result.bugs.add(reason)

            baseline_results = allowed_results(baseline_test)
            fission_results = allowed_results(fission_test)
            result_is_regression = is_regression(baseline_results, fission_results)

            if baseline_results != fission_results:
                logger.debug(
                    "  %s %s %s %s"
                    % (test_id, baseline_results, fission_results, result_is_regression)
                )

            if result_is_regression:
                test_result.add_regression(platform, baseline_results, fission_results)

            for (
                name,
                baseline_subtest_meta,
            ) in baseline_test._test_metadata.subtests.items():
                fission_subtest_meta = baseline_test._test_metadata.subtests[name]
                if name not in test_result.subtest_results:
                    test_result.add_subtest(name)

                subtest_result = test_result.subtest_results[name]

                baseline_bug = get_meta_prop(baseline_test, name, "bug")
                fission_bug = get_meta_prop(fission_test, name, "bug")
                if fission_bug and fission_bug != baseline_bug:
                    subtest_result.bugs.add(fission_bug)

                if bool(fission_subtest_meta.disabled) and not bool(
                    baseline_subtest_meta.disabled
                ):
                    subtest_result.disabled.add(platform)
                    if maybe_bug_re.match(fission_subtest_meta.disabled):
                        subtest_result.bugs.add(fission_subtest_meta.disabled)

                baseline_results = allowed_results(baseline_test, name)
                fission_results = allowed_results(fission_test, name)

                result_is_regression = is_regression(baseline_results, fission_results)

                if baseline_results != fission_results:
                    logger.debug(
                        "    %s %s %s %s %s"
                        % (
                            test_id,
                            name,
                            baseline_results,
                            fission_results,
                            result_is_regression,
                        )
                    )

                if result_is_regression:
                    subtest_result.add_regression(
                        platform, baseline_results, fission_results
                    )

    test_results = {
        test_id: result
        for test_id, result in test_results.items()
        if include_result(result)
    }

    if kwargs["all_json"] is not None:
        write_all(test_results, kwargs["all_json"])

    if kwargs["untriaged"] is not None:
        write_untriaged(test_results, kwargs["untriaged"])


def write_all(test_results, path):
    json_data = {test_id: result.to_json() for test_id, result in test_results.items()}

    dir_name = os.path.dirname(path)
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)

    with open(path, "w") as f:
        json.dump(json_data, f, indent=2)


def write_untriaged(test_results, path):
    dir_name = os.path.dirname(path)
    if not os.path.exists(dir_name):
        os.makedirs(dir_name)

    data = sorted(
        (test_id, result)
        for test_id, result in test_results.items()
        if not result.is_triaged()
    )

    with open(path, "w") as f:
        for test_id, result in data:
            f.write(test_id + "\n")
            for name, subtest_result in sorted(result.subtest_results.items()):
                if not subtest_result.is_triaged():
                    f.write("    %s\n" % name)
