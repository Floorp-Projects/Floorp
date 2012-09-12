# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class FedoraBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        os.system("sudo yum groupinstall 'Development Tools' 'Development Libraries' 'GNOME Software Development'")
        os.system("sudo yum install mercurial autoconf213 glibc-static libstdc++-static yasm wireless-tools-devel mesa-libGL-devel alsa-lib-devel libXt-devel")
