#!/usr/bin/env python

from __future__ import absolute_import

from time import sleep

import mozunit


def test_run_interactive(runner, create_thread):
    """Bug 965183: Run process in interactive mode and call wait()"""
    runner.start(interactive=True)

    thread = create_thread(runner, timeout=2)
    thread.start()

    # This is a blocking call. So the process should be killed by the thread
    runner.wait()
    thread.join()
    assert not runner.is_running()


def test_stop_interactive(runner):
    """Bug 965183: Explicitely stop process in interactive mode"""
    runner.start(interactive=True)
    runner.stop()


def test_wait_after_process_finished(runner):
    """Wait after the process has been stopped should not raise an error"""
    runner.start(interactive=True)
    sleep(1)
    runner.process_handler.kill()

    returncode = runner.wait(1)

    assert returncode not in [None, 0]
    assert runner.process_handler is not None


if __name__ == '__main__':
    mozunit.main()
