# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from datetime import datetime

from six.moves.urllib.parse import quote

from marionette_driver.by import By
from marionette_driver.date_time_value import DateTimeValue
from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class TestDateTime(MarionetteTestCase):
    def test_set_date(self):
        self.marionette.navigate(inline("<input id='date-test' type='date'/>"))

        element = self.marionette.find_element(By.ID, "date-test")
        dt_value = DateTimeValue(element)
        dt_value.date = datetime(1998, 6, 2)
        self.assertEqual("1998-06-02", element.get_property("value"))

    def test_set_time(self):
        self.marionette.navigate(inline("<input id='time-test' type='time'/>"))

        element = self.marionette.find_element(By.ID, "time-test")
        dt_value = DateTimeValue(element)
        dt_value.time = datetime(1998, 11, 19, 9, 8, 7)
        self.assertEqual("09:08:07", element.get_property("value"))
