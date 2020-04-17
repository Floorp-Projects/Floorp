#!/usr/bin/env python
import os
import mozunit
import json

from mozperftest.tests.support import get_running_env, temp_file
from mozperftest.environment import METRICS

HERE = os.path.dirname(__file__)


def test_metrics():
    mach_cmd, metadata, env = get_running_env()
    runs = []

    def _run_process(*args, **kw):
        runs.append((args, kw))

    mach_cmd.run_process = _run_process
    metrics = env.layers[METRICS]
    env.set_arg("tests", [os.path.join(HERE, "example.js")])

    # XXX why do I have to set defaults?
    env.set_arg("prefix", "")
    env.set_arg("perfherder", True)

    metadata.set_result(os.path.join(HERE, "browsertime-results"))

    with temp_file() as output:
        env.set_arg("output", output)
        with metrics as m:
            m(metadata)
        output_file = metadata.get_output()
        with open(output_file) as f:
            output = json.loads(f.read())

    # XXX more checks
    assert output["suites"][0]["value"] > 0


if __name__ == "__main__":
    mozunit.main()
