#!/usr/bin/env python
import os
import mozunit
import mock

from mozperftest.tests.support import EXAMPLE_TEST, get_running_env, temp_dir
from mozperftest.environment import METRICS
from mozperftest.utils import silence


HERE = os.path.dirname(__file__)


@mock.patch("mozperftest.metrics.common.validate_intermediate_results")
def test_console_output(*mocked):
    with temp_dir() as tempdir:
        options = {
            "perfherder": True,
            "perfherder-prefix": "",
            "console": True,
            "output": tempdir,
        }
        mach_cmd, metadata, env = get_running_env(**options)
        runs = []

        def _run_process(*args, **kw):
            runs.append((args, kw))

        mach_cmd.run_process = _run_process
        metrics = env.layers[METRICS]
        env.set_arg("tests", [EXAMPLE_TEST])
        bt_res = os.path.join(HERE, "browsertime-results", "browsertime.json")

        res = {"name": "name", "results": [bt_res]}
        metadata.add_result(res)

        with metrics as console, silence():
            console(metadata)


if __name__ == "__main__":
    mozunit.main()
