# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase

class TestFail(MarionetteTestCase):
    def test_fails(self):
        # this test is supposed to fail!
        self.assertEquals(True, False)
