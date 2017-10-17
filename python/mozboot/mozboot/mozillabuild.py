# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import os
import sys
import subprocess
import tempfile

from mozboot.base import BaseBootstrapper

class MozillaBuildBootstrapper(BaseBootstrapper):
    '''Bootstrapper for MozillaBuild to install rustup.'''
    def __init__(self, no_interactive=False):
        BaseBootstrapper.__init__(self, no_interactive=no_interactive)
        print("mach bootstrap is not fully implemented in MozillaBuild")

    def which(self, name):
        return BaseBootstrapper.which(self, name + '.exe')

    def install_system_packages(self):
        pass

    def ensure_mercurial_modern(self):
        # Overrides default implementation to always run pip because.
        print('Running pip to ensure Mercurial is up-to-date...')
        self.run([self.which('pip'), 'install', '--upgrade', 'Mercurial'])
        return True, True

    def upgrade_python(self, current):
        pass

    def install_browser_packages(self):
        pass

    def install_browser_artifact_mode_packages(self):
        pass

    def install_mobile_android_packages(self):
        pass

    def install_mobile_android_artifact_mode_packages(self):
        pass

    def ensure_stylo_packages(self, state_dir, checkout_root):
        import stylo
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.WINDOWS)

    def _update_package_manager(self):
        pass

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pip_install(self, *packages):
        pip_dir = os.path.join(os.environ['MOZILLABUILD'], 'python', 'Scripts', 'pip.exe')
        command = [pip_dir, 'install', '--upgrade']
        command.extend(packages)
        self.run(command)

