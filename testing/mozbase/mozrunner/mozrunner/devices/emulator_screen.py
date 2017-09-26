# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import


class EmulatorScreen(object):
    """Class for screen related emulator commands."""

    SO_PORTRAIT_PRIMARY = 'portrait-primary'
    SO_PORTRAIT_SECONDARY = 'portrait-secondary'
    SO_LANDSCAPE_PRIMARY = 'landscape-primary'
    SO_LANDSCAPE_SECONDARY = 'landscape-secondary'

    def __init__(self, emulator):
        self.emulator = emulator

    def initialize(self):
        self.orientation = self.SO_PORTRAIT_PRIMARY

    def _get_raw_orientation(self):
        """Get the raw value of the current device orientation."""
        response = self.emulator._run_telnet('sensor get orientation')

        return response[0].split('=')[1].strip()

    def _set_raw_orientation(self, data):
        """Set the raw value of the specified device orientation."""
        self.emulator._run_telnet('sensor set orientation %s' % data)

    def get_orientation(self):
        """Get the current device orientation.

        Returns;
            orientation -- Orientation of the device. One of:
                            SO_PORTRAIT_PRIMARY - system buttons at the bottom
                            SO_PORTRIAT_SECONDARY - system buttons at the top
                            SO_LANDSCAPE_PRIMARY - system buttons at the right
                            SO_LANDSCAPE_SECONDARY - system buttons at the left

        """
        data = self._get_raw_orientation()

        if data == '0:-90:0':
            orientation = self.SO_PORTRAIT_PRIMARY
        elif data == '0:90:0':
            orientation = self.SO_PORTRAIT_SECONDARY
        elif data == '0:0:90':
            orientation = self.SO_LANDSCAPE_PRIMARY
        elif data == '0:0:-90':
            orientation = self.SO_LANDSCAPE_SECONDARY
        else:
            raise ValueError('Unknown orientation sensor value: %s.' % data)

        return orientation

    def set_orientation(self, orientation):
        """Set the specified device orientation.

        Args
            orientation -- Orientation of the device. One of:
                            SO_PORTRAIT_PRIMARY - system buttons at the bottom
                            SO_PORTRIAT_SECONDARY - system buttons at the top
                            SO_LANDSCAPE_PRIMARY - system buttons at the right
                            SO_LANDSCAPE_SECONDARY - system buttons at the left
        """
        orientation = SCREEN_ORIENTATIONS[orientation]

        if orientation == self.SO_PORTRAIT_PRIMARY:
            data = '0:-90:0'
        elif orientation == self.SO_PORTRAIT_SECONDARY:
            data = '0:90:0'
        elif orientation == self.SO_LANDSCAPE_PRIMARY:
            data = '0:0:90'
        elif orientation == self.SO_LANDSCAPE_SECONDARY:
            data = '0:0:-90'
        else:
            raise ValueError('Invalid orientation: %s' % orientation)

        self._set_raw_orientation(data)

    orientation = property(get_orientation, set_orientation)


SCREEN_ORIENTATIONS = {"portrait": EmulatorScreen.SO_PORTRAIT_PRIMARY,
                       "landscape": EmulatorScreen.SO_LANDSCAPE_PRIMARY,
                       "portrait-primary": EmulatorScreen.SO_PORTRAIT_PRIMARY,
                       "landscape-primary": EmulatorScreen.SO_LANDSCAPE_PRIMARY,
                       "portrait-secondary": EmulatorScreen.SO_PORTRAIT_SECONDARY,
                       "landscape-secondary": EmulatorScreen.SO_LANDSCAPE_SECONDARY}
