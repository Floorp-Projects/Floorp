# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import threading
from time import sleep

import mozprofile
import mozrunner
import pytest
from moztest.selftest import fixtures


@pytest.fixture
def profile():
    return mozprofile.FirefoxProfile()


@pytest.fixture
def get_binary():
    if 'BROWSER_PATH' in os.environ:
        os.environ['GECKO_BINARY_PATH'] = os.environ['BROWSER_PATH']

    def inner(app):
        if app != 'firefox':
            pytest.xfail(reason="{} support not implemented".format(app))

        binary = fixtures.binary()
        if not binary:
            pytest.skip("could not find a {} binary".format(app))
        return binary
    return inner


@pytest.fixture
def runner(profile, get_binary):
    binary = get_binary('firefox')
    return mozrunner.FirefoxRunner(binary, profile=profile)


class RunnerThread(threading.Thread):
    def __init__(self, runner, start=False, timeout=1):
        threading.Thread.__init__(self)
        self.runner = runner
        self.timeout = timeout
        self.do_start = start

    def run(self):
        sleep(self.timeout)
        if self.do_start:
            self.runner.start()
        else:
            self.runner.stop()


@pytest.fixture
def create_thread():
    threads = []

    def inner(*args, **kwargs):
        thread = RunnerThread(*args, **kwargs)
        threads.append(thread)
        return thread

    yield inner

    for thread in threads:
        thread.join()
