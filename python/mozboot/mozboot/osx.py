# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper

class OSXBootstrapper(BaseBootstrapper):
    def __init__(self, major, minor, point):
        BaseBootstrapper.__init__(self)

        if major == 10 and minor < 6:
            raise Exception('OS X 10.6 or above is required.')

    def install_system_packages(self):
        raise NotImplementedError('OS X bootstrap not yet implemented.')


