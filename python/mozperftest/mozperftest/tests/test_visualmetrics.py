#!/usr/bin/env python
import json
import os
from unittest import mock

import mozunit
import pytest

from mozperftest.environment import METRICS
from mozperftest.tests.support import (
    BT_DATA_VIDEO,
    EXAMPLE_TEST,
    get_running_env,
    temp_file,
)

VM_RES = {
    "SpeedIndex": 1031,
    "FirstVisualChange": 533,
    "LastVisualChange": 3166,
    "VisualProgress": (
        "0=0, 533=63, 700=63, 733=63, 900=63, 933=63, 1233=54,"
        "1333=54, 1366=56, 1500=56, 1633=96, 1800=96, 1933=96,"
        "2133=96, 2200=96, 2366=96, 2533=96, 2566=96, 2600=96,"
        "2733=96, 2833=96, 2933=96, 3000=96, 3133=96,3166=100"
    ),
    "videoRecordingStart": 0,
}


def get_res(*args, **kw):
    return json.dumps(VM_RES)


def mocked_executable():
    return ("ok", "ok")


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
@mock.patch("mozperftest.metrics.visualmetrics.which", new=lambda path: "ok")
@mock.patch("mozbuild.nodeutil.find_node_executable", new=mocked_executable)
@mock.patch("subprocess.check_output", new=get_res)
def test_visual_metrics(device):
    os.environ["VISUALMETRICS_PY"] = ""
    mach_cmd, metadata, env = get_running_env(
        visualmetrics=True,
        perfherder=True,
        verbose=True,
        tests=[EXAMPLE_TEST],
    )
    metrics = env.layers[METRICS]

    metadata.add_result({"results": str(BT_DATA_VIDEO.parent), "name": "browsertime"})

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m:
            metadata = m(metadata)

        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # Check some metadata
    assert output["application"]["name"] == "firefox"
    visual_metrics = [i["name"] for i in output["suites"][1]["subtests"]]
    assert "VisualProgress96" in visual_metrics


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
@mock.patch("mozperftest.metrics.visualmetrics.which", new=lambda path: None)
def test_visual_metrics_no_ffmpeg(device):
    os.environ["VISUALMETRICS_PY"] = ""
    mach_cmd, metadata, env = get_running_env(
        visualmetrics=True,
        perfherder=True,
        verbose=True,
        tests=[EXAMPLE_TEST],
    )
    metrics = env.layers[METRICS]
    metadata.add_result({"results": str(BT_DATA_VIDEO.parent), "name": "browsertime"})

    with pytest.raises(FileNotFoundError):
        with metrics as m:
            metadata = m(metadata)


if __name__ == "__main__":
    mozunit.main()
