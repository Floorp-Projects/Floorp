# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

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


class MozillaBuildBootstrapper(BaseBootstrapper):
    '''Bootstrapper for MozillaBuild to install rustup.'''
    def __init__(self, no_interactive=False, no_system_changes=False):
        BaseBootstrapper.__init__(self, no_interactive=no_interactive,
                                  no_system_changes=no_system_changes)
        print("mach bootstrap is not fully implemented in MozillaBuild")

    def which(self, name, *extra_search_dirs):
        return BaseBootstrapper.which(self, name + '.exe', *extra_search_dirs)

    def install_system_packages(self):
        pass

    def upgrade_mercurial(self, current):
        # Mercurial upstream sometimes doesn't upload wheels, and building
        # from source requires MS Visual C++ 9.0. So we force pip to install
        # the last version that comes with wheels.
        self.pip_install('mercurial', '--only-binary', 'mercurial')

    def upgrade_python(self, current):
        pass

    def install_browser_packages(self):
        pass

    def install_browser_artifact_mode_packages(self):
        pass

    def install_mobile_android_packages(self):
        pass

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_mobile_android_packages(self, artifact_mode=False):
        # Get java path from registry key
        import _winreg

        key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE,
                              r'SOFTWARE\JavaSoft\Java Development Kit\1.8')
        java_path, regtype = _winreg.QueryValueEx(key, 'JavaHome')
        _winreg.CloseKey(key)
        os.environ['PATH'] = \
            '{}{}{}'.format(os.path.join(java_path, 'bin'), os.pathsep, os.environ['PATH'])
        self.ensure_java()

        from mozboot import android
        android.ensure_android('windows', artifact_mode=artifact_mode,
                               no_interactive=self.no_interactive)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android
        android.suggest_mozconfig('windows', artifact_mode=artifact_mode)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        self.suggest_mobile_android_mozconfig(artifact_mode=True)

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
        pass

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pip_install(self, *packages):
        pip_dir = os.path.join(os.environ['MOZILLABUILD'], 'python', 'Scripts', 'pip.exe')
        command = [pip_dir, 'install', '--upgrade']
        command.extend(packages)
        self.run(command)
