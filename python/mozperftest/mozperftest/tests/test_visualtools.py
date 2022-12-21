#!/usr/bin/env python
import os
from unittest import mock

import mozunit
import pytest

from mozperftest.test.browsertime.visualtools import get_dependencies, xvfb
from mozperftest.utils import temporary_env


@mock.patch(
    "mozperftest.test.browsertime.visualtools.find_executable", new=lambda name: "Xvfb"
)
def test_xvfb(*mocked):
    with temporary_env(DISPLAY="ME"):
        with mock.patch("subprocess.Popen") as mocked, xvfb():
            mocked.assert_called()
        assert os.environ["DISPLAY"] == "ME"


@mock.patch(
    "mozperftest.test.browsertime.visualtools.find_executable", new=lambda name: "Xvfb"
)
def test_xvfb_env(*mocked):
    with temporary_env(DISPLAY=None):
        with mock.patch("subprocess.Popen") as mocked, xvfb():
            mocked.assert_called()
        assert "DISPLAY" not in os.environ


@mock.patch(
    "mozperftest.test.browsertime.visualtools.find_executable", new=lambda name: None
)
def test_xvfb_none(*mocked):
    with pytest.raises(FileNotFoundError), xvfb():
        pass


def test_get_dependencies():
    # Making sure we get a list on all supported platforms.
    # If we miss one, this raises a KeyError.
    get_dependencies()


if __name__ == "__main__":
    mozunit.main()
