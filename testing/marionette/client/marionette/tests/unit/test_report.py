# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from marionette_test import MarionetteTestCase, expectedFailure, skip


class TestReport(MarionetteTestCase):

    def test_pass(self):
        assert True

    def test_fail(self):
        assert False

    @skip('Skip Message')
    def test_skip(self):
        assert False

    @expectedFailure
    def test_expected_fail(self):
        assert False

    @expectedFailure
    def test_unexpected_pass(self):
        assert True

    def test_error(self):
        raise Exception()
