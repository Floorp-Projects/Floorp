#!/usr/bin/env python

from __future__ import absolute_import

import mozunit
import pytest

from mozrunner import RunnerNotStartedError


def test_errors_before_start(runner):
    """Bug 965714: Not started errors before start() is called"""

    with pytest.raises(RunnerNotStartedError):
        runner.is_running()

    with pytest.raises(RunnerNotStartedError):
        runner.returncode

    with pytest.raises(RunnerNotStartedError):
        runner.wait()


if __name__ == '__main__':
    mozunit.main()
