#!/usr/bin/env python
from unittest import mock

import mozunit

from mozperftest.environment import METRICS
from mozperftest.tests.support import BT_DATA, EXAMPLE_TEST, get_running_env
from mozperftest.utils import silence, temp_dir


@mock.patch("mozperftest.metrics.common.validate_intermediate_results")
def test_console_output(*mocked):
    with temp_dir() as tempdir:
        options = {
            "console-prefix": "",
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
        res = {"name": "name", "results": [str(BT_DATA)]}
        metadata.add_result(res)

        with metrics as console, silence():
            console(metadata)


if __name__ == "__main__":
    mozunit.main()
