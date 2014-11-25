# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from errors import MarionetteException
from marionette_test import MarionetteTestCase

class TestSetFrameTimeout(MarionetteTestCase):

    def test_set_valid_frame_timeout(self):
        self.marionette.set_frame_timeout(10000)

    def test_set_invalid_frame_timeout(self):
        with self.assertRaisesRegexp(MarionetteException, "Not a number"):
            self.marionette.set_frame_timeout("timeout")
