# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import ctypes
import os
import sys
import subprocess

from mozboot.base import BaseBootstrapper


def is_aarch64_host():
    from ctypes import wintypes
    kernel32 = ctypes.windll.kernel32
    IMAGE_FILE_MACHINE_UNKNOWN = 0
    IMAGE_FILE_MACHINE_ARM64 = 0xAA64

    try:
        iswow64process2 = kernel32.IsWow64Process2
    except Exception:
        # If we can't access the symbol, we know we're not on aarch64.
        return False

    currentProcess = kernel32.GetCurrentProcess()
    processMachine = wintypes.USHORT(IMAGE_FILE_MACHINE_UNKNOWN)
    nativeMachine = wintypes.USHORT(IMAGE_FILE_MACHINE_UNKNOWN)

    gotValue = iswow64process2(currentProcess,
                               ctypes.byref(processMachine),
                               ctypes.byref(nativeMachine))
    # If this call fails, we have no idea.
    if not gotValue:
        return False

    return nativeMachine.value == IMAGE_FILE_MACHINE_ARM64


class WindowsBootstrapper(BaseBootstrapper):
    '''Bootstrapper for msys2 based environments for building in Windows.'''

    SYSTEM_PACKAGES = [
        'mingw-w64-x86_64-make',
        'mingw-w64-x86_64-python2-pip',
        'mingw-w64-x86_64-perl',
        'patch',
        'patchutils',
        'diffutils',
        'autoconf2.13',
        'tar',
        'zip',
        'unzip',
        'mingw-w64-x86_64-toolchain',  # TODO: Remove when Mercurial is installable from a wheel.
        'mingw-w64-i686-toolchain'
    ]

    BROWSER_PACKAGES = [
        'mingw-w64-x86_64-nasm',
        'mingw-w64-x86_64-yasm',
        'mingw-w64-i686-nsis'
    ]

    MOBILE_ANDROID_COMMON_PACKAGES = [
        'wget'
    ]

    def __init__(self, **kwargs):
        if 'MOZ_WINDOWS_BOOTSTRAP' not in os.environ or os.environ['MOZ_WINDOWS_BOOTSTRAP'] != '1':
            raise NotImplementedError('Bootstrap support for Windows is under development. For '
                                      'now use MozillaBuild to set up a build environment on '
                                      'Windows. If you are testing Windows Bootstrap support, '
                                      'try `export MOZ_WINDOWS_BOOTSTRAP=1`')
        BaseBootstrapper.__init__(self, **kwargs)
        if not self.which('pacman'):
            raise NotImplementedError('The Windows bootstrapper only works with msys2 with '
                                      'pacman. Get msys2 at http://msys2.github.io/')
        print('Using an experimental bootstrapper for Windows.')

    def which(self, name, *extra_search_dirs):
        return BaseBootstrapper.which(self, name + '.exe', *extra_search_dirs)

    def install_system_packages(self):
        self.pacman_install(*self.SYSTEM_PACKAGES)

    def upgrade_mercurial(self, current):
        self.pip_install('mercurial')

    def upgrade_python(self, current):
        self.pacman_install('mingw-w64-x86_64-python2')

    def install_browser_packages(self):
        self.pacman_install(*self.BROWSER_PACKAGES)

    def install_mobile_android_packages(self):
        raise NotImplementedError('We do not support building Android on Windows. Sorry!')

    def install_mobile_android_artifact_mode_packages(self):
        raise NotImplementedError('We do not support building Android on Windows. Sorry!')

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        from mozboot import static_analysis
        self.install_toolchain_static_analysis(
            state_dir, checkout_root, static_analysis.WINDOWS_CLANG_TIDY)

    def ensure_stylo_packages(self, state_dir, checkout_root):
        # On-device artifact builds are supported; on-device desktop builds are not.
        if is_aarch64_host():
            raise Exception('You should not be performing desktop builds on an '
                            'AArch64 device.  If you want to do artifact builds '
                            'instead, please choose the appropriate artifact build '
                            'option when beginning bootstrap.')

        from mozboot import stylo
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.WINDOWS_CLANG)
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.WINDOWS_CBINDGEN)

    def ensure_nasm_packages(self, state_dir, checkout_root):
        from mozboot import nasm
        self.install_toolchain_artifact(state_dir, checkout_root, nasm.WINDOWS_NASM)

    def ensure_node_packages(self, state_dir, checkout_root):
        from mozboot import node
        # We don't have native aarch64 node available, but aarch64 windows
        # runs x86 binaries, so just use the x86 packages for such hosts.
        node_artifact = node.WIN32 if is_aarch64_host() else node.WIN64
        self.install_toolchain_artifact(
            state_dir, checkout_root, node_artifact)

    def _update_package_manager(self):
        self.pacman_update()

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pacman_update(self):
        command = ['pacman', '--sync', '--refresh']
        self.run(command)

    def pacman_upgrade(self):
        command = ['pacman', '--sync', '--refresh', '--sysupgrade']
        self.run(command)

    def pacman_install(self, *packages):
        command = ['pacman', '--sync', '--needed']
        if self.no_interactive:
            command.append('--noconfirm')

        command.extend(packages)
        self.run(command)

    def pip_install(self, *packages):
        command = ['pip', 'install', '--upgrade']
        command.extend(packages)
        self.run(command)
