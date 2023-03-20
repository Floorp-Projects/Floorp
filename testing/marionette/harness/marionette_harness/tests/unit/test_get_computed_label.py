# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver import By, errors
from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestGetComputedLabel(MarionetteTestCase):
    def test_can_get_computed_label(self):
        self.marionette.navigate(inline("<label for=b>foo<label><input id=b>"))
        computed_label = self.marionette.find_element(By.ID, "b").computed_label
        self.assertEqual(computed_label, "foo")

    def test_get_computed_label_no_such_element(self):
        self.marionette.navigate(inline("<div id=a>"))
        element = self.marionette.find_element(By.ID, "a")
        element.id = "b"
        with self.assertRaises(errors.NoSuchElementException):
            element.computed_label
