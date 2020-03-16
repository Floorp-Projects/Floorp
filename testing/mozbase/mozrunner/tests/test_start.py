#!/usr/bin/env python

from __future__ import absolute_import

from time import sleep

import mozunit
from mock import patch
from pytest import raises

from mozrunner import RunnerNotStartedError


def test_start_process(runner):
    """Start the process and test properties"""
    assert runner.process_handler is None

    runner.start()

    assert runner.is_running()
    assert runner.process_handler is not None


def test_start_process_called_twice(runner):
    """Start the process twice and test that first process is gone"""
    runner.start()
    # Bug 925480
    # Make a copy until mozprocess can kill a specific process
    process_handler = runner.process_handler

    runner.start()

    try:
        assert process_handler.wait(1) not in [None, 0]
    finally:
        process_handler.kill()


def test_start_with_timeout(runner):
    """Start the process and set a timeout"""
    runner.start(timeout=0.1)
    sleep(1)

    assert not runner.is_running()


def test_start_with_outputTimeout(runner):
    """Start the process and set a timeout"""
    runner.start(outputTimeout=0.1)
    sleep(1)

    assert not runner.is_running()


def test_fail_to_start(runner):
    with patch('mozprocess.ProcessHandler.__init__') as ph_mock:
        ph_mock.side_effect = Exception('Boom!')
        with raises(RunnerNotStartedError):
            runner.start(outputTimeout=0.1)
            sleep(1)


if __name__ == '__main__':
    mozunit.main()
