#!/usr/bin/env python
import os
import mozunit
import mock
import shutil

from mozperftest.tests.support import get_running_env
from mozperftest.environment import BROWSER


HERE = os.path.dirname(__file__)


def fetch(self, url):
    return os.path.join(HERE, "fetched_artifact.zip")


@mock.patch(
    "mozperftest.browser.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
def test_browser():
    mach_cmd, metadata, env = get_running_env()
    browser = env.layers[BROWSER]
    env.set_arg("tests", [os.path.join(HERE, "example.js")])

    try:
        with browser as b:
            b(metadata)
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    assert mach_cmd.run_process.call_count == 1
    # XXX more checks
    assert mach_cmd.run_process.call_args[0][-1][-1] == os.path.join(HERE, "example.js")


if __name__ == "__main__":
    mozunit.main()
