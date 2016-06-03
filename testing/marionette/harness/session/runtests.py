# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from session.session_test import SessionTestCase, SessionJSTestCase
from session.runner import BaseSessionTestRunner, BaseSessionArguments
from marionette.runtests import MarionetteHarness, cli


class SessionTestRunner(BaseSessionTestRunner):
    def __init__(self, **kwargs):
        BaseSessionTestRunner.__init__(self, **kwargs)
        self.test_handlers = [SessionTestCase, SessionJSTestCase]


class SessionArguments(BaseSessionArguments):
    def __init__(self, **kwargs):
        BaseSessionArguments.__init__(self, **kwargs)


if __name__ == "__main__":
    cli(runner_class=SessionTestRunner, parser_class=SessionArguments,
        harness_class=MarionetteHarness, testcase_class=SessionTestCase, args=None)
