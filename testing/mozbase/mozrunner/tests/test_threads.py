#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozunit


def test_process_start_via_thread(runner, create_thread):
    """Start the runner via a thread"""
    thread = create_thread(runner, True, 2)

    thread.start()
    thread.join()

    assert runner.is_running()


def test_process_stop_via_multiple_threads(runner, create_thread):
    """Stop the runner via multiple threads"""
    runner.start()
    threads = []
    for i in range(5):
        thread = create_thread(runner, False, 5)
        threads.append(thread)
        thread.start()

    # Wait until the process has been stopped by another thread
    for thread in threads:
        thread.join()
    returncode = runner.wait(1)

    assert returncode not in [None, 0]
    assert runner.returncode == returncode
    assert runner.process_handler is not None
    assert runner.wait(2) == returncode


def test_process_post_stop_via_thread(runner, create_thread):
    """Stop the runner and try it again with a thread a bit later"""
    runner.start()
    thread = create_thread(runner, False, 5)
    thread.start()

    # Wait a bit to start the application gets started
    runner.wait(1)
    returncode = runner.stop()
    thread.join()

    assert returncode not in [None, 0]
    assert runner.returncode == returncode
    assert runner.process_handler is not None
    assert runner.wait(2) == returncode


if __name__ == '__main__':
    mozunit.main()
