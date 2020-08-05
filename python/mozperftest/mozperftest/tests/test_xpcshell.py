import json
from unittest import mock
import shutil

import pytest

from mozperftest.tests.support import get_running_env, EXAMPLE_XPCSHELL_TEST, temp_file
from mozperftest.environment import TEST, SYSTEM, METRICS
from mozperftest.test.xpcshell import XPCShellTestError


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


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTests)
def test_xpcshell_metrics(*mocked):
    mach_cmd, metadata, env = get_running_env(flavor="xpcshell")

    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]
    env.set_arg("tests", [str(EXAMPLE_XPCSHELL_TEST)])

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


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTestsFail)
def test_xpcshell_metrics_fail(*mocked):
    mach_cmd, metadata, env = get_running_env(flavor="xpcshell")
    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]
    env.set_arg("tests", [str(EXAMPLE_XPCSHELL_TEST)])

    try:
        with sys as s, xpcshell as x, pytest.raises(XPCShellTestError):
            x(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)


@mock.patch("runxpcshelltests.XPCShellTests", new=XPCShellTests)
def test_xpcshell_perfherder(*mocked):
    mach_cmd, metadata, env = get_running_env(
        flavor="xpcshell", perfherder=True, xpcshell_cycles=10
    )

    sys = env.layers[SYSTEM]
    xpcshell = env.layers[TEST]
    env.set_arg("tests", [str(EXAMPLE_XPCSHELL_TEST)])
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
    assert output["framework"]["name"] == "xpcshell"

    # Check some numbers in our data
    assert len(output["suites"]) == 1
    assert len(output["suites"][0]["subtests"]) == 3
    assert output["suites"][0]["value"] > 0

    for subtest in output["suites"][0]["subtests"]:
        assert subtest["name"].startswith("metrics")
