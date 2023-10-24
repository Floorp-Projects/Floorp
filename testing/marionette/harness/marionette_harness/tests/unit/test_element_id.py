# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
from urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.errors import NoSuchElementException, InvalidSelectorException
from marionette_driver.marionette import WebElement

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


id_html = inline("<p id=foo></p>")


class TestElementID(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.timeout.implicit = 0

    def test_id_is_valid_uuid(self):
        self.marionette.navigate(id_html)
        el = self.marionette.find_element(By.TAG_NAME, "p")
        uuid_regex = re.compile(
            "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
        )
        self.assertIsNotNone(
            re.search(uuid_regex, el.id),
            "UUID for the WebElement is not valid. ID is {}".format(el.id),
        )

    def test_id_identical_for_the_same_element(self):
        self.marionette.navigate(id_html)
        found = self.marionette.find_element(By.ID, "foo")
        self.assertIsInstance(found, WebElement)

        found_again = self.marionette.find_element(By.ID, "foo")
        self.assertEqual(found_again, found)

    def test_id_unique_per_session(self):
        self.marionette.navigate(id_html)
        found = self.marionette.find_element(By.ID, "foo")
        self.assertIsInstance(found, WebElement)

        self.marionette.delete_session()
        self.marionette.start_session()

        found_again = self.marionette.find_element(By.ID, "foo")
        self.assertNotEqual(found_again.id, found.id)
