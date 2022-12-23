# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestElementSize(MarionetteTestCase):
    def test_payload(self):
        self.marionette.navigate(inline("""<a href="#">link</a>"""))
        rect = self.marionette.find_element(By.LINK_TEXT, "link").rect
        self.assertTrue(rect["x"] > 0)
        self.assertTrue(rect["y"] > 0)
        self.assertTrue(rect["width"] > 0)
        self.assertTrue(rect["height"] > 0)
