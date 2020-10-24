#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozunit


def test_wait_while_running(runner):
    """Wait for the process while it is running"""
    runner.start()
    returncode = runner.wait(1)

    assert runner.is_running()
    assert returncode is None
    assert runner.returncode == returncode
    assert runner.process_handler is not None


def test_wait_after_process_finished(runner):
    """Bug 965714: wait() after stop should not raise an error"""
    runner.start()
    runner.process_handler.kill()

    returncode = runner.wait(1)

    assert returncode not in [None, 0]
    assert runner.process_handler is not None


if __name__ == '__main__':
    mozunit.main()
