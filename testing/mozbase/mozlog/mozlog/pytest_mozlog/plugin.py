# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozlog
import time


def pytest_addoption(parser):
    # We can't simply use mozlog.commandline.add_logging_group(parser) here because
    # Pytest's parser doesn't have the add_argument_group method Mozlog expects.
    group = parser.getgroup('mozlog')

    for name, (_class, _help) in mozlog.commandline.log_formatters.iteritems():
        group.addoption('--log-{0}'.format(name), action='append', help=_help)

    formatter_options = mozlog.commandline.fmt_options.iteritems()
    for name, (_class, _help, formatters, action) in formatter_options:
        for formatter in formatters:
            if formatter in mozlog.commandline.log_formatters:
                group.addoption(
                    '--log-{0}-{1}'.format(formatter, name),
                    action=action,
                    help=_help)


def pytest_configure(config):
    # If using pytest-xdist for parallelization, only register plugin on master process
    if not hasattr(config, 'slaveinput'):
        config.pluginmanager.register(MozLog())


class MozLog(object):

    def __init__(self):
        self.results = {}
        self.start_time = int(time.time() * 1000)  # in ms for Mozlog compatibility

    def pytest_configure(self, config):
        mozlog.commandline.setup_logging('pytest', config.known_args_namespace,
                                         defaults={}, allow_unused_options=True)
        self.logger = mozlog.get_default_logger(component='pytest')

    def pytest_sessionstart(self, session):
        '''Called before test collection; records suite start time to log later'''
        self.start_time = int(time.time() * 1000)  # in ms for Mozlog compatibility

    def pytest_collection_modifyitems(self, items):
        '''Called after test collection is completed, just before tests are run (suite start)'''
        self.logger.suite_start(tests=items, time=self.start_time)

    def pytest_sessionfinish(self, session, exitstatus):
        self.logger.suite_end()

    def pytest_runtest_logstart(self, nodeid, location):
        self.logger.test_start(test=nodeid)

    def pytest_runtest_logreport(self, report):
        '''Called 3 times per test (setup, call, teardown), indicated by report.when'''
        test = report.nodeid
        status = expected = 'PASS'
        message = stack = None
        if hasattr(report, 'wasxfail'):
            # Pytest reporting for xfail tests is somewhat counterinutitive:
            # If an xfail test fails as expected, its 'call' report has .skipped,
            # so we record status FAIL (== expected) and log an expected result.
            # If an xfail unexpectedly passes, the 'call' report has .failed (Pytest 2)
            # or .passed (Pytest 3), so we leave status as PASS (!= expected)
            # to log an unexpected result.
            expected = 'FAIL'
            if report.skipped:  # indicates expected failure (passing test)
                status = 'FAIL'
        elif report.failed:
            status = 'FAIL' if report.when == 'call' else 'ERROR'
            crash = report.longrepr.reprcrash  # here longrepr is a ReprExceptionInfo
            message = "{0} (line {1})".format(crash.message, crash.lineno)
            stack = report.longrepr.reprtraceback
        elif report.skipped:  # indicates true skip
            status = expected = 'SKIP'
            message = report.longrepr[-1]  # here longrepr is a tuple (file, lineno, reason)
        if status != expected or expected != 'PASS':
            self.results[test] = (status, expected, message, stack)
        if report.when == 'teardown':
            defaults = ('PASS', 'PASS', None, None)
            status, expected, message, stack = self.results.get(test, defaults)
            self.logger.test_end(test=test, status=status, expected=expected,
                                 message=message, stack=stack)
