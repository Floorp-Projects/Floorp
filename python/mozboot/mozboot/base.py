# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import subprocess
import sys

class BaseBootstrapper(object):
    """Base class for system bootstrappers."""

    def install_system_packages(self):
        raise NotImplemented('%s must implement install_system_packages()' %
            __name__)

    def which(self, name):
        """Python implementation of which.

        It returns the path of an executable or None if it couldn't be found.
        """
        for path in os.environ['PATH'].split(os.pathsep):
            test = os.path.join(path, name)
            if os.path.exists(test) and os.access(test, os.X_OK):
                return test

        return None

    def run_as_root(self, command):
        if os.geteuid() != 0:
            command.insert(0, 'sudo')

        print('Executing as root:', subprocess.list2cmdline(command))

        subprocess.check_call(command, stdin=sys.stdin)

    def yum_install(self, *packages):
        command = ['yum', 'install']
        command.extend(packages)

        self.run_as_root(command)

    def yum_groupinstall(self, *packages):
        command = ['yum', 'groupinstall']
        command.extend(packages)

        self.run_as_root(command)

    def apt_install(self, *packages):
        command = ['apt-get', 'install']
        command.extend(packages)

        self.run_as_root(command)
