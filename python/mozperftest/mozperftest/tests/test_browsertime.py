#!/usr/bin/env python
import os
import mozunit
from unittest import mock
import shutil
import string
import random

import pytest

from mozperftest.tests.support import get_running_env, EXAMPLE_TEST
from mozperftest.environment import TEST, SYSTEM
from mozperftest.test.browsertime import add_options
from mozperftest.test.browsertime.runner import (
    NodeException,
    matches,
    extract_browser_name,
)
from mozperftest.utils import silence, temporary_env


HERE = os.path.dirname(__file__)


def fetch(self, url):
    return os.path.join(HERE, "fetched_artifact.zip")


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
def test_browser(*mocked):
    mach_cmd, metadata, env = get_running_env(
        android=True,
        android_app_name="something",
        browsertime_geckodriver="GECKODRIVER",
        browsertime_iterations=1,
        browsertime_extra_options="one=1,two=2",
        tests=[EXAMPLE_TEST],
    )

    sys = env.layers[SYSTEM]
    browser = env.layers[TEST]
    try:
        with sys as s, browser as b, silence():
            b(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)
    assert mach_cmd.run_process.call_count == 1

    # Make sure all arguments are of type str
    for option in mach_cmd.run_process.call_args[0][0]:
        assert isinstance(option, str)

    cmd = " ".join(mach_cmd.run_process.call_args[0][0])
    assert EXAMPLE_TEST in cmd
    assert "--firefox.geckodriverPath GECKODRIVER" in cmd
    assert "--one 1" in cmd
    assert "--two 2" in cmd

    results = metadata.get_results()
    assert len(results) == 1
    assert set(list(results[0].keys())) - set(["name", "results"]) == set()
    assert results[0]["name"] == "Example"


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
def test_browser_failed(*mocked):
    mach_cmd, metadata, env = get_running_env(
        android=True,
        android_app_name="something",
        browsertime_geckodriver="GECKODRIVER",
        browsertime_iterations=1,
        browsertime_extra_options="one=1,two=2",
        tests=[EXAMPLE_TEST],
    )
    # set the return value to 1 to simulate a node failure
    mach_cmd.run_process.return_value = 1
    browser = env.layers[TEST]
    sys = env.layers[SYSTEM]

    with sys as s, browser as b, silence(), pytest.raises(NodeException):
        b(s(metadata))


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
def test_browser_desktop(*mocked):
    mach_cmd, metadata, env = get_running_env(
        browsertime_iterations=1,
        browsertime_extra_options="one=1,two=2",
        tests=[EXAMPLE_TEST],
    )
    browser = env.layers[TEST]
    sys = env.layers[SYSTEM]

    try:
        with sys as s, browser as b, silence():
            # just checking that the setup_helper property gets
            # correctly initialized
            browsertime = browser.layers[-1]
            assert browsertime.setup_helper is not None
            helper = browsertime.setup_helper
            assert browsertime.setup_helper is helper

            b(s(metadata))
    finally:
        shutil.rmtree(mach_cmd._mach_context.state_dir)

    assert mach_cmd.run_process.call_count == 1
    cmd = " ".join(mach_cmd.run_process.call_args[0][0])
    # check that --firefox.binaryPath is set automatically
    assert "--firefox.binaryPath" in cmd


def test_add_options():
    mach_cmd, metadata, env = get_running_env()
    options = [("one", 1), ("two", 2)]
    add_options(env, options)
    extra = env.get_arg("browsertime-extra-options")
    assert "one=1" in extra
    assert "two=2" in extra


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch("mozperftest.test.browsertime.runner.BrowsertimeRunner.setup_helper")
def test_install_url(*mocked):
    url = "https://here/tarball/" + "".join(
        [random.choice(string.hexdigits[:-6]) for c in range(40)]
    )
    mach, metadata, env = get_running_env(
        browsertime_install_url=url,
        tests=[EXAMPLE_TEST],
    )
    browser = env.layers[TEST]
    sys = env.layers[SYSTEM]

    try:
        with sys as s, temporary_env(MOZ_AUTOMATION="1"), browser as b, silence():
            b(s(metadata))
    finally:
        shutil.rmtree(mach._mach_context.state_dir)

    assert mach.run_process.call_count == 1


@mock.patch("mozperftest.test.browsertime.runner.install_package")
@mock.patch(
    "mozperftest.test.noderunner.NodeRunner.verify_node_install", new=lambda x: True
)
@mock.patch("mozbuild.artifact_cache.ArtifactCache.fetch", new=fetch)
@mock.patch(
    "mozperftest.test.browsertime.runner.BrowsertimeRunner._setup_node_packages",
    new=lambda x, y: None,
)
def test_install_url_bad(*mocked):
    mach, metadata, env = get_running_env(
        browsertime_install_url="meh",
        tests=[EXAMPLE_TEST],
    )
    browser = env.layers[TEST]
    sys = env.layers[SYSTEM]

    with pytest.raises(ValueError):
        try:
            with sys as s, browser as b, silence():
                b(s(metadata))
        finally:
            shutil.rmtree(mach._mach_context.state_dir)


def test_matches():
    args = ["arg1=1", "--arg2=value2"]

    assert matches(args, "arg1")
    assert not matches(args, "arg3")


def test_extract_browser_name():
    args = ["arg1=1", "--arg2=value2", "--browser=me", "--zome"]
    assert extract_browser_name(args) == "me"


if __name__ == "__main__":
    mozunit.main()
