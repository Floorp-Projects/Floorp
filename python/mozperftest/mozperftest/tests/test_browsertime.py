#!/usr/bin/env python
import os
import mozunit
import mock
import shutil

from mozperftest.tests.support import get_running_env
from mozperftest.environment import BROWSER
from mozperftest.browser.browsertime import add_options
from mozperftest.utils import silence


HERE = os.path.dirname(__file__)


def fetch(self, url):
    return os.path.join(HERE, "fetched_artifact.zip")


@mock.patch(
    "mozperftest.browser.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.browser.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
def test_browser():
    mach_cmd, metadata, env = get_running_env()
    browser = env.layers[BROWSER]
    env.set_arg("tests", [os.path.join(HERE, "example.js")])

    try:
        with browser as b, silence():
            b(metadata)
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    assert mach_cmd.run_process.call_count == 1
    # XXX more checks
    assert mach_cmd.run_process.call_args[0][-1][-1] == os.path.join(HERE, "example.js")


def test_add_options():
    mach_cmd, metadata, env = get_running_env()
    options = [("one", 1), ("two", 2)]
    add_options(env, options)
    extra = env.get_arg("browsertime-extra-options")
    assert "one=1" in extra
    assert "two=2" in extra


if __name__ == "__main__":
    mozunit.main()
