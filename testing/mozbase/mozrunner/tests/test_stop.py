#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import signal

import mozunit


def test_stop_process(runner):
    """Stop the process and test properties"""
    runner.start()
    returncode = runner.stop()

    assert not runner.is_running()
    assert returncode not in [None, 0]
    assert runner.returncode == returncode
    assert runner.process_handler is not None
    assert runner.wait(1) == returncode


def test_stop_before_start(runner):
    """Stop the process before it gets started should not raise an error"""
    runner.stop()


def test_stop_process_custom_signal(runner):
    """Stop the process via a custom signal and test properties"""
    runner.start()
    returncode = runner.stop(signal.SIGTERM)

    assert not runner.is_running()
    assert returncode not in [None, 0]
    assert runner.returncode == returncode
    assert runner.process_handler is not None
    assert runner.wait(1) == returncode


if __name__ == '__main__':
    mozunit.main()
