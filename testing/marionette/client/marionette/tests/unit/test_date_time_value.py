# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_test import MarionetteTestCase
from datetime import datetime
from date_time_value import DateTimeValue

class TestDateTime(MarionetteTestCase):

    def test_set_date(self):
        test_html = self.marionette.absolute_url("datetimePage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "date-test")
        dt_value = DateTimeValue(element)
        dt_value.date = datetime(1998, 6, 2)
        self.assertEqual(element.get_attribute("value"), "1998-06-02")

    def test_set_time(self):
        test_html = self.marionette.absolute_url("datetimePage.html")
        self.marionette.navigate(test_html)

        element = self.marionette.find_element("id", "time-test")
        dt_value = DateTimeValue(element)
        dt_value.time = datetime(1998, 11, 19, 9, 8, 7)
        self.assertEqual(element.get_attribute("value"), "09:08:07")
