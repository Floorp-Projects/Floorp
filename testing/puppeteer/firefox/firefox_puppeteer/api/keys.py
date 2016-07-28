# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import marionette_driver


class Keys(marionette_driver.keys.Keys):
    """Proxy to marionette's keys with an "accel" provided for convenience
    testing across platforms."""

    def __init__(self, marionette_getter):
        self.marionette_getter = marionette_getter

        caps = self.marionette_getter().session_capabilities
        self.isDarwin = caps['platformName'] == 'darwin'

    @property
    def ACCEL(self):
        return self.META if self.isDarwin else self.CONTROL
