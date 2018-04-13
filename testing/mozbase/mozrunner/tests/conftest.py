# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import threading
from time import sleep

import mozrunner
import pytest
from moztest.selftest import fixtures


@pytest.fixture(scope='session')
def get_binary():
    if 'BROWSER_PATH' in os.environ:
        os.environ['GECKO_BINARY_PATH'] = os.environ['BROWSER_PATH']

    def inner(app):
        if app not in ('chrome', 'firefox'):
            pytest.xfail(reason="{} support not implemented".format(app))

        if app == 'firefox':
            binary = fixtures.binary()
        elif app == 'chrome':
            binary = os.environ.get('CHROME_BINARY_PATH')

        if not binary:
            pytest.skip("could not find a {} binary".format(app))
        return binary
    return inner


@pytest.fixture(params=['firefox', 'chrome'])
def runner(request, get_binary):
    app = request.param
    binary = get_binary(app)

    cmdargs = ['--headless']
    if app == 'chrome':
        # prevents headless chrome from exiting after loading the page
        cmdargs.append('--remote-debugging-port=9222')
        # only needed on Windows, but no harm in specifying it everywhere
        cmdargs.append('--disable-gpu')
    runner = mozrunner.runners[app](binary, cmdargs=cmdargs)
    runner.app = app
    yield runner
    runner.stop()


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
