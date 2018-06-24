# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import marionette_driver


class Keys(marionette_driver.keys.Keys):
    """
    Proxy to Marionette's keys with an "accel" provided for convenience
    testing across platforms.
    """

    def __init__(self, marionette):
        self.is_mac = marionette.session_capabilities["platformName"] == "mac"

    @property
    def ACCEL(self):
        return self.META if self.is_mac else self.CONTROL
