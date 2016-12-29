# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
        self.install_rustup()

    def install_rustup(self):
        try:
            rustup_init = tempfile.gettempdir() + '/rustup-init.exe'
            self.http_download_and_save(
                    'https://static.rust-lang.org/rustup/archive/0.2.0/i686-pc-windows-msvc/rustup-init.exe',
                    rustup_init,
                    'a45ab7462b567dacddaf6e9e48bb43a1b9c1db4404ba77868f7d6fc685282a46')
            self.run([rustup_init, '--no-modify-path', '--default-host',
                'x86_64-pc-windows-msvc', '--default-toolchain', 'stable', '-y'])
            mozillabuild_dir = os.environ['MOZILLABUILD']

            with open(mozillabuild_dir + 'msys/etc/profile.d/profile-rustup.sh', 'wb') as f:
                f.write('#!/bash/sh\n')
                f.write('if test -n "$MOZILLABUILD"; then\n')
                f.write('    WIN_HOME=$(command cd "$HOME" && pwd)\n')
                f.write('    PATH="$WIN_HOME/.cargo/bin:$PATH"\n')
                f.write('    export PATH\n')
                f.write('fi')
        finally:
            try:
                os.remove(rustup_init)
            except FileNotFoundError:
                pass

    def upgrade_mercurial(self, current):
        self.pip_install('mercurial')

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

    def _update_package_manager(self):
        pass

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pip_install(self, *packages):
        pip_dir = os.path.join(os.environ['MOZILLABUILD'], 'python', 'Scripts', 'pip.exe')
        command = [pip_dir, 'install', '--upgrade']
        command.extend(packages)
        self.run(command)

