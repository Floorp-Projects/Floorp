# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

class DateTimeValue(object):
    """
    Interface for setting the value of HTML5 "date" and "time" input elements.

    Simple usage example:

    ::

        element = marionette.find_element("id", "date-test")
        dt_value = DateTimeValue(element)
        dt_value.date = datetime(1998, 6, 2)

    """

    def __init__(self, element):
        self.element = element

    @property
    def date(self):
        """
        Retrieve the element's string value
        """
        return self.element.get_attribute('value')

    # As per the W3C "date" element specification
    # (http://dev.w3.org/html5/markup/input.date.html), this value is formatted
    # according to RFC 3339: http://tools.ietf.org/html/rfc3339#section-5.6
    @date.setter
    def date(self, date_value):
        self.element.send_keys(date_value.strftime('%Y-%m-%d'))

    @property
    def time(self):
        """
        Retrieve the element's string value
        """
        return self.element.get_attribute('value')

    # As per the W3C "time" element specification
    # (http://dev.w3.org/html5/markup/input.time.html), this value is formatted
    # according to RFC 3339: http://tools.ietf.org/html/rfc3339#section-5.6
    @time.setter
    def time(self, time_value):
        self.element.send_keys(time_value.strftime('%H:%M:%S'))

