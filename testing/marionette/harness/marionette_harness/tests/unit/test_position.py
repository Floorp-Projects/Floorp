# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By

from marionette_harness import MarionetteTestCase


class TestPosition(MarionetteTestCase):

    def test_should_get_element_position_back(self):
        test_url = self.marionette.absolute_url('rectangles.html')
        self.marionette.navigate(test_url)

        r2 = self.marionette.find_element(By.ID, "r2")
        location = r2.rect
        self.assertEqual(11, location['x'])
        self.assertEqual(10, location['y'])
