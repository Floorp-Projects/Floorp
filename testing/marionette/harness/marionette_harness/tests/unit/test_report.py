# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import (
    expectedFailure,
    MarionetteTestCase,
    skip,
    unexpectedSuccess,
)


class TestReport(MarionetteTestCase):
    def test_pass(self):
        assert True

    @skip("Skip Message")
    def test_skip(self):
        assert False

    @expectedFailure
    def test_error(self):
        raise Exception()

    @unexpectedSuccess
    def test_unexpected_pass(self):
        assert True
