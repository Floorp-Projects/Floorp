#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from pathlib import Path
from unittest import mock

import mozunit
import pytest

from mozperftest.environment import MachEnvironment
from mozperftest.hooks import Hooks
from mozperftest.layers import Layer
from mozperftest.tests.support import get_running_env, requests_content

HERE = Path(__file__).parent.resolve()


def _get_env(hooks_path):
    return MachEnvironment(mock.MagicMock(), hooks=Hooks(mock.MagicMock(), hooks_path))


def test_run_hooks():
    env = _get_env(Path(HERE, "data", "hook.py"))
    assert env.hooks.run("doit", env) == "OK"


def test_bad_hooks():
    with pytest.raises(IOError):
        _get_env("Idontexists")


doit = [b"def doit(*args, **kw):\n", b"    return 'OK'\n"]


@mock.patch("mozperftest.utils.requests.get", requests_content(doit))
def test_run_hooks_url():
    env = _get_env("http://somewhere/hooks.py")
    assert env.hooks.run("doit", env) == "OK"


def test_layers():
    env = MachEnvironment(mock.MagicMock())
    assert env.get_layer("browsertime").name == "browsertime"


def test_context():
    mach, metadata, env = get_running_env()
    env.layers = [mock.MagicMock(), mock.MagicMock(), mock.MagicMock()]
    with env:
        env.run(metadata)


class FailureException(Exception):
    pass


class Failure(Layer):
    user_exception = True

    def run(self, metadata):
        raise FailureException()


def create_mock():
    m = mock.Mock()

    # need to manually set those
    def enter(self):
        self.setup()
        return self

    def exit(self, type, value, traceback):
        self.teardown()

    m.__enter__ = enter
    m.__exit__ = exit
    m.__call__ = mock.Mock()
    return m


def test_exception_return():
    # the last layer is not called, the error is swallowed
    hooks = str(Path(HERE, "data", "hook.py"))
    mach, metadata, env = get_running_env(hooks=hooks)
    last_layer = create_mock()
    env.layers = [create_mock(), Failure(env, mach), last_layer]
    with env:
        env.run(metadata)
    last_layer.assert_not_called()


def test_exception_resume():
    # the last layer is called, the error is swallowed
    hooks = str(Path(HERE, "data", "hook_resume.py"))
    mach, metadata, env = get_running_env(hooks=hooks)
    last_layer = create_mock()
    env.layers = [create_mock(), Failure(env, mach), last_layer]
    with env:
        env.run(metadata)
    last_layer.assert_called()


def test_exception_no_user_exception():
    # the last layer is called, the error is raised
    # because user_exception = False
    hooks = str(Path(HERE, "data", "hook_resume.py"))
    mach, metadata, env = get_running_env(hooks=hooks)
    last_layer = create_mock()
    f = Failure(env, mach)
    f.user_exception = False
    env.layers = [create_mock(), f, last_layer]
    with env, pytest.raises(FailureException):
        env.run(metadata)
    last_layer._call__.assert_not_called()


def test_exception_raised():
    # the error is raised
    hooks = str(Path(HERE, "data", "hook_raises.py"))
    mach, metadata, env = get_running_env(hooks=hooks)
    last_layer = create_mock()
    env.layers = [create_mock(), Failure(env, mach), last_layer]
    with env, pytest.raises(FailureException):
        env.run(metadata)
    last_layer.__call__.assert_not_called()


def test_metrics_last():
    mach, metadata, env = get_running_env()

    system = create_mock()
    browser = create_mock()

    # Check that the metrics layer is entered after
    # other have finished and that the other layers
    # were only called once
    class M:
        def __enter__(self):
            system.setup.assert_called_once()
            browser.setup.assert_called_once()
            system.teardown.assert_called_once()
            browser.teardown.assert_called_once()
            return self

        def __exit__(self, *args, **kw):
            return

        def __call__(self, metadata):
            return

    env.layers = [system, browser, M()]
    with env:
        env.run(metadata)


if __name__ == "__main__":
    mozunit.main()
