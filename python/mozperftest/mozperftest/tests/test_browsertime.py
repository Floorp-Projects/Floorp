#!/usr/bin/env python
import os
import mozunit
import mock
import shutil

from mozperftest.browser import pick_browser
from mozperftest.tests.support import get_running_env

HERE = os.path.dirname(__file__)


def fetch(self, url):
    return os.path.join(HERE, "fetched_artifact.zip")


@mock.patch(
    "mozperftest.browser.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
def test_browser():
    mach_cmd, metadata = get_running_env()
    runs = []

    def _run_process(*args, **kw):
        runs.append((args, kw))

    mach_cmd.run_process = _run_process
    metadata["mach_args"] = {"tests": [os.path.join(HERE, "example.js")]}

    try:
        with pick_browser("script", mach_cmd) as env:
            env(metadata)
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    args = runs[0][0]
    # kwargs = runs[0][1]
    # XXX more checks
    assert args[-1][-1] == os.path.join(HERE, "example.js")


if __name__ == "__main__":
    mozunit.main()
