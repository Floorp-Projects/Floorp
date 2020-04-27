#!/usr/bin/env python
import os
import mozunit

from mozperftest.tests.support import get_running_env, temp_dir
from mozperftest.environment import METRICS
from mozperftest.utils import silence


HERE = os.path.dirname(__file__)


def test_console_output():
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
        env.set_arg("tests", [os.path.join(HERE, "example.js")])
        metadata.set_result(os.path.join(HERE, "browsertime-results"))

        with metrics as console, silence():
            console(metadata)


if __name__ == "__main__":
    mozunit.main()
