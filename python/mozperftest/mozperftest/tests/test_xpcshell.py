import json
import shutil
from unittest import mock

import pytest

from mozperftest import utils
from mozperftest.environment import METRICS, SYSTEM, TEST
from mozperftest.test import xpcshell
from mozperftest.test.xpcshell import XPCShellTestError
from mozperftest.tests.support import (
    EXAMPLE_XPCSHELL_TEST,
    MOZINFO,
    get_running_env,
    temp_file,
)


class XPCShellTests:
    def __init__(self, log):
        self.log = log

    def runTests(self, args):
        self.log.suite_start("suite start")
        self.log.test_start("test start")
        self.log.process_output("1234", "line", "command")
        self.log.log_raw({"action": "something"})
        self.log.log_raw({"action": "log", "message": "message"})

        # these are the metrics sent by the scripts
        self.log.log_raw(
            {
                "action": "log",
                "message": '"perfMetrics"',
                "extra": {"metrics1": 1, "metrics2": 2},
            }
        )

        self.log.log_raw(
            {"action": "log", "message": '"perfMetrics"', "extra": {"metrics3": 3}}
        )

        self.log.test_end("test end")
        self.log.suite_end("suite end")
        return True


class XPCShellTestsFail(XPCShellTests):
    def runTests(self, args):
        return False


class XPCShellTestsNoPerfMetrics:
    def __init__(self, log):
        self.log = log

    def runTests(self, args):
        self.log.suite_start("suite start")
        self.log.test_start("test start")
        self.log.process_output("1234", "line", "command")
        self.log.log_raw({"action": "something"})
        self.log.log_raw({"action": "log", "message": "message"})

        self.log.test_end("test end")
        self.log.suite_end("suite end")
        return True


def running_env(**kw):
    return get_running_env(flavor="xpcshell", xpcshell_mozinfo=MOZINFO, **kw)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTests)
def test_xpcshell_metrics(*mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_XPCSHELL_TEST)])

    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]

    try:
        with sys as s, xpcshell as x:
            x(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    res = metadata.get_results()
    assert len(res) == 1
    assert res[0]["name"] == "test_xpcshell.js"
    results = res[0]["results"]

    assert results[0]["name"] == "metrics1"
    assert results[0]["values"] == [1]


def _test_xpcshell_fail(err, *mocked):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_XPCSHELL_TEST)])
    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]
    try:
        with sys as s, xpcshell as x, pytest.raises(err):
            x(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTestsFail)
def test_xpcshell_metrics_fail(*mocked):
    return _test_xpcshell_fail(XPCShellTestError, mocked)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTestsNoPerfMetrics)
def test_xpcshell_no_perfmetrics(*mocked):
    return _test_xpcshell_fail(utils.NoPerfMetricsError, *mocked)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTests)
def test_xpcshell_perfherder(*mocked):
    return _test_xpcshell_perfherder(*mocked)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTests)
def test_xpcshell_perfherder_on_try(*mocked):
    old = utils.ON_TRY
    utils.ON_TRY = xpcshell.ON_TRY = not utils.ON_TRY

    try:
        return _test_xpcshell_perfherder(*mocked)
    finally:
        utils.ON_TRY = old
        xpcshell.ON_TRY = old


def _test_xpcshell_perfherder(*mocked):
    mach_cmd, metadata, env = running_env(
        perfherder=True, xpcshell_cycles=10, tests=[str(EXAMPLE_XPCSHELL_TEST)]
    )

    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]
    metrics = env.layers[METRICS]

    with temp_file() as output:
        env.set_arg("output", output)
        try:
            with sys as s, xpcshell as x, metrics as m:
                m(x(s(metadata)))
        finally:
            shutil.rmtree(mach_cmd._mach_context.state_dir)

        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Check some metadata
    assert output["application"]["name"] == "firefox"
    assert output["framework"]["name"] == "mozperftest"

    # Check some numbers in our data
    assert len(output["suites"]) == 1
    assert len(output["suites"][0]["subtests"]) == 3
    assert "value" not in output["suites"][0]
    assert any(r > 0 for r in output["suites"][0]["subtests"][0]["replicates"])

    for subtest in output["suites"][0]["subtests"]:
        assert subtest["name"].startswith("metrics")
