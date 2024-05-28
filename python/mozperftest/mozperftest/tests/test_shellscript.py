#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from unittest import mock

import mozunit
import pytest

from mozperftest.environment import TEST
from mozperftest.test.shellscript import ShellScriptRunner, UnknownScriptError
from mozperftest.tests.support import EXAMPLE_SHELL_TEST, get_running_env


def running_env(**kw):
    return get_running_env(flavor="custom-script", **kw)


def test_shell_script_metric_parsing():
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_SHELL_TEST)])

    runner = ShellScriptRunner(env, mach_cmd)
    line_handler = runner.line_handler_wrapper()

    line_handler(mock.MagicMock(), "don't parse line".encode())
    assert len(runner.metrics) == 0

    line_handler(
        mock.MagicMock(), 'perfMetrics: [{{"name": "metric1", "values": []}}]'.encode()
    )
    line_handler(
        mock.MagicMock(), 'perfMetrics: {{"name": "metric2", "values": [1]}}'.encode()
    )
    assert len(runner.metrics) == 2

    parsed_metrics = runner.parse_metrics()
    assert len(parsed_metrics) == 2
    assert parsed_metrics[0]["name"] == "metric1"
    assert parsed_metrics[1]["name"] == "metric2"
    assert len(parsed_metrics[1]["values"]) == 1


@mock.patch("mozperftest.test.shellscript.ShellScriptRunner.parse_metrics")
@mock.patch("mozperftest.test.shellscript.mozprocess.run_and_wait")
def test_shell_script(mocked_mozprocess, mocked_metrics):
    mach_cmd, metadata, env = running_env(tests=[str(EXAMPLE_SHELL_TEST)])

    mocked_metrics.return_value = [
        {"name": "metric1", "values": [1, 2]},
    ]

    customscript = env.layers[TEST]
    metadata.binary = "a_binary"
    with customscript as c:
        c(metadata)

    res = metadata.get_results()
    assert len(res) == 1
    assert "metric1" == res[0]["results"][0]["name"]


def test_shell_script_unknown_type_error():
    runner = ShellScriptRunner(mock.MagicMock(), mock.MagicMock())
    with pytest.raises(UnknownScriptError):
        mocked_metadata = mock.MagicMock()
        mocked_metadata.script = {"filename": "unknown"}
        runner.run(mocked_metadata)


@mock.patch("mozperftest.test.shellscript.signal")
@mock.patch("mozperftest.test.shellscript.os")
@mock.patch("mozperftest.test.shellscript.platform")
def test_shell_script_kill(mocked_platform, mocked_os, mocked_signal):
    runner = ShellScriptRunner(mock.MagicMock(), mock.MagicMock())
    mocked_proc = mock.MagicMock()

    mocked_platform.system.return_value = "linux"
    runner.kill(mocked_proc)

    mocked_proc.wait.assert_called_once()
    mocked_os.killpg.assert_called_once()

    mocked_platform.system.return_value = "windows"
    runner.kill(mocked_proc)

    mocked_proc.send_signal.assert_called_once()
    assert mocked_proc.wait.call_count == 2


if __name__ == "__main__":
    mozunit.main()
