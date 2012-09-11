# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import platform
import sys

from mozboot.osx import OSXBootstrapper
from mozboot.ubuntu import UbuntuBootstrapper


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def bootstrap(self):
        cls = None
        args = {}

        if sys.platform.startswith('linux'):
            distro, version, dist_id = platform.linux_distribution()

            if distro == 'Ubuntu':
                cls = UbuntuBootstrapper
            else:
                raise NotImplementedError('Bootstrap support for this Linux '
                                          'distro not yet available.')

            args['version'] = version
            args['dist_id'] = dist_id

        elif sys.platform.startswith('darwin'):
            # TODO Support Darwin platforms that aren't OS X.
            major, minor, point = map(int, platform.mac_ver()[0].split('.'))

            cls = OSXBootstrapper
            args['major'] = major
            args['minor'] = minor
            args['point'] = point

        if cls is None:
            raise NotImplementedError('Bootstrap support is not yet available '
                                      'for your OS.')

        instance = cls(**args)
        instance.install_system_packages()
