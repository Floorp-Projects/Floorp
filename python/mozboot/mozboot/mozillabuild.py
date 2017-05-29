# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import os
import sys
import subprocess
import tempfile

from mozboot.base import BaseBootstrapper

STYLO_MOZCONFIG = '''
To enable Stylo in your builds, paste the lines between the chevrons
(>>> and <<<) into your mozconfig file:

<<<
ac_add_options --enable-stylo

ac_add_options --with-libclang-path="{state_dir}/clang/bin"
ac_add_options --with-clang-path="{state_dir}/clang/bin/clang.exe"
>>>
'''

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
            _, cargo_bin = self.cargo_home()
            rustup = os.path.join(cargo_bin, 'rustup')
            self.run([rustup, 'target', 'add', 'i686-pc-windows-msvc'])
        finally:
            try:
                os.remove(rustup_init)
            except OSError as e:
                if e.errno == errno.ENOENT:
                    pass
                else:
                    raise

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

    def suggest_browser_mozconfig(self):
        if self.stylo:
            print(STYLO_MOZCONFIG.format(state_dir=self.state_dir))

    def ensure_stylo_packages(self, state_dir, checkout_root):
        import stylo
        self.install_tooltool_clang_package(state_dir, checkout_root, stylo.WINDOWS)

    def _update_package_manager(self):
        pass

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pip_install(self, *packages):
        pip_dir = os.path.join(os.environ['MOZILLABUILD'], 'python', 'Scripts', 'pip.exe')
        command = [pip_dir, 'install', '--upgrade']
        command.extend(packages)
        self.run(command)

