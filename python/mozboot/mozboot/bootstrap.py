# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import platform
import sys

from mozboot.centos import CentOSBootstrapper
from mozboot.fedora import FedoraBootstrapper
from mozboot.mint import MintBootstrapper
from mozboot.osx import OSXBootstrapper
from mozboot.ubuntu import UbuntuBootstrapper


FINISHED = '''
Your system should be ready to build Firefox! If you have not already,
obtain a copy of the source code by running:

    hg clone https://hg.mozilla.org/mozilla-central

Or, if you prefer Git:

    git clone git://github.com/mozilla/mozilla-central.git
'''


class Bootstrapper(object):
    """Main class that performs system bootstrap."""

    def bootstrap(self):
        cls = None
        args = {}

        if sys.platform.startswith('linux'):
            distro, version, dist_id = platform.linux_distribution()

            if distro == 'CentOS':
                cls = CentOSBootstrapper
            elif distro == 'Fedora':
                cls = FedoraBootstrapper
            elif distro == 'Mint':
                cls = MintBootstrapper
            elif distro == 'Ubuntu':
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

        print(FINISHED)
