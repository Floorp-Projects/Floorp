# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozdevice
import os


class B2GTestCaseMixin(object):

    # TODO: add methods like 'restart b2g'
    def __init__(self, *args, **kwargs):
        self._device_manager = None

    def get_device_manager(self, *args, **kwargs):
        if not self._device_manager:
            dm_type = os.environ.get('DM_TRANS', 'adb')
            if dm_type == 'adb':
                self._device_manager = mozdevice.DeviceManagerADB(**kwargs)
            elif dm_type == 'sut':
                host = os.environ.get('TEST_DEVICE')
                if not host:
                    raise Exception('Must specify host with SUT!')
                self._device_manager = mozdevice.DeviceManagerSUT(host=host)
            else:
                raise Exception('Unknown device manager type: %s' % dm_type)
        return self._device_manager

    @property
    def device_manager(self):
        return self.get_device_manager()
