# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform

from mozboot.base import BaseBootstrapper

class CentOSBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        kern = platform.uname()
        os.system("sudo yum groupinstall 'Development Tools' 'Development Libraries' 'GNOME Software Development'")
        os.system("sudo yum install mercurial autoconf213 glibc-static libstdc++-static yasm wireless-tools-devel mesa-libGL-devel alsa-lib-devel libXt-devel")
        os.system("sudo yum install gtk2-devel")
        os.system("sudo yum install dbus-glib-devel")

        if ('x86_64' in kern[2]):
            os.system("sudo rpm -ivh http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.x86_64.rpm")
        else:
            os.system("sudo rpm -ivh http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.i686.rpm")

        os.system("sudo yum install curl-devel")

