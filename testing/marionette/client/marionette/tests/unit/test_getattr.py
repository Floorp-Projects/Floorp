# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase


class TestGetAttribute(MarionetteTestCase):
    def test_getAttribute(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        l = self.marionette.find_element("id", "mozLink")
        self.assertEqual("mozLink", l.get_attribute("id"))
