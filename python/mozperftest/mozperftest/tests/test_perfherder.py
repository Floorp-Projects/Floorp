#!/usr/bin/env python
import os
import mozunit
import json

from mozperftest.metrics import pick_metrics
from mozperftest.tests.support import get_running_env

HERE = os.path.dirname(__file__)


def test_metrics():
    mach_cmd, metadata = get_running_env()
    runs = []

    def _run_process(*args, **kw):
        runs.append((args, kw))

    mach_cmd.run_process = _run_process
    metadata["mach_args"] = {"tests": [os.path.join(HERE, "example.js")]}
    metadata["results"] = os.path.join(HERE, "browsertime-results")

    with pick_metrics("script", mach_cmd) as env:
        env(metadata)

    with open(metadata["output"]) as f:
        output = json.loads(f.read())

    os.remove(metadata["output"])
    # XXX more checks
    assert output["suites"][0]["value"] > 0


if __name__ == "__main__":
    mozunit.main()
