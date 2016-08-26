# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase


class TestTitle(MarionetteTestCase):
    def test_get_html_title(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        self.assertEqual('Marionette Test', self.marionette.title)
