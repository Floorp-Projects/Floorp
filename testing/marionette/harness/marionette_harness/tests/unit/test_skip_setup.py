# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_harness import MarionetteTestCase, SkipTest


class TestSetUpSkipped(MarionetteTestCase):

    testVar = {'test':'SkipTest'}

    def setUp(self):
        try:
            self.testVar['email']
        except KeyError:
            raise SkipTest('email key not present in dict, skip ...')
        MarionetteTestCase.setUp(self)

    def test_assert(self):
        assert True

class TestSetUpNotSkipped(MarionetteTestCase):

    testVar = {'test':'SkipTest'}

    def setUp(self):
        try:
            self.testVar['test']
        except KeyError:
            raise SkipTest('email key not present in dict, skip ...')
        MarionetteTestCase.setUp(self)

    def test_assert(self):
        assert True

