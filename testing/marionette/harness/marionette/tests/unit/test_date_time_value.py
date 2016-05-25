# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from datetime import datetime

from marionette import MarionetteTestCase
from marionette_driver.date_time_value import DateTimeValue
from marionette_driver.by import By


class TestDateTime(MarionetteTestCase):
    def test_set_date(self):
        test_html = self.marionette.absolute_url("datetimePage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "date-test")
        dt_value = DateTimeValue(element)
        dt_value.date = datetime(1998, 6, 2)
        self.assertEqual("1998-06-02", element.get_property("value"))

    def test_set_time(self):
        test_html = self.marionette.absolute_url("datetimePage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element(By.ID, "time-test")
        dt_value = DateTimeValue(element)
        dt_value.time = datetime(1998, 11, 19, 9, 8, 7)
        self.assertEqual("09:08:07", element.get_property("value"))
