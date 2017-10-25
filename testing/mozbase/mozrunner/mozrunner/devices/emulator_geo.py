# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import


class EmulatorGeo(object):

    def __init__(self, emulator):
        self.emulator = emulator

    def set_default_location(self):
        self.lon = -122.08769
        self.lat = 37.41857
        self.set_location(self.lon, self.lat)

    def set_location(self, lon, lat):
        self.emulator._run_telnet('geo fix %0.5f %0.5f' % (self.lon, self.lat))
