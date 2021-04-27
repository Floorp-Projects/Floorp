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

    gotValue = iswow64process2(
        currentProcess, ctypes.byref(processMachine), ctypes.byref(nativeMachine)
    )
    # If this call fails, we have no idea.
    if not gotValue:
        return False

    return nativeMachine.value == IMAGE_FILE_MACHINE_ARM64


def get_is_windefender_disabled():
    import winreg

    try:
        with winreg.OpenKeyEx(
            winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Defender"
        ) as windefender_key:
            is_antivirus_disabled, _ = winreg.QueryValueEx(
                windefender_key, "DisableAntiSpyware"
            )
            # is_antivirus_disabled is either 0 (False) or 1 (True)
            return bool(is_antivirus_disabled)
    except FileNotFoundError:
        return True


def get_windefender_exclusion_paths():
    import winreg

    paths = []
    try:
        with winreg.OpenKeyEx(
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Windows Defender\Exclusions\Paths",
        ) as exclusions_key:
            _, values_count, __ = winreg.QueryInfoKey(exclusions_key)
            for i in range(0, values_count):
                path, _, __ = winreg.EnumValue(exclusions_key, i)
                paths.append(path)
    except FileNotFoundError:
        pass

    return paths


def is_windefender_affecting_srcdir(srcdir):
    if get_is_windefender_disabled():
        return False

    # When there's a match, but path cases aren't the same between srcdir and exclusion_path,
    # commonpath will use the casing of the first path provided.
    # To avoid surprises here, we normcase(...) so we don't get unexpected breakage if we change
    # the path order.
    srcdir = os.path.normcase(os.path.abspath(srcdir))

    try:
        exclusion_paths = get_windefender_exclusion_paths()
    except OSError as e:
        if e.winerror == 5:
            # A version of Windows 10 released in 2021 raises an "Access is denied"
            # error (ERROR_ACCESS_DENIED == 5) to un-elevated processes when they
            # query Windows Defender's exclusions. Skip the exclusion path checking.
            return
        raise

    for exclusion_path in exclusion_paths:
        exclusion_path = os.path.normcase(os.path.abspath(exclusion_path))
        try:
            if os.path.commonpath([exclusion_path, srcdir]) == exclusion_path:
                # exclusion_path is an ancestor of srcdir
                return False
        except ValueError:
            # ValueError: Paths don't have the same drive - can't be ours
            pass
    return True


class MozillaBuildBootstrapper(BaseBootstrapper):
    """Bootstrapper for MozillaBuild to install rustup."""

    INSTALL_PYTHON_GUIDANCE = (
        "Python is provided by MozillaBuild; ensure your MozillaBuild "
        "installation is up to date."
    )

    def __init__(self, no_interactive=False, no_system_changes=False):
        BaseBootstrapper.__init__(
            self, no_interactive=no_interactive, no_system_changes=no_system_changes
        )

    def validate_environment(self, srcdir):
        if self.application.startswith("mobile_android"):
            print(
                "WARNING!!! Building Firefox for Android on Windows is not "
                "fully supported. See https://bugzilla.mozilla.org/show_bug."
                "cgi?id=1169873 for details.",
                file=sys.stderr,
            )

        if is_windefender_affecting_srcdir(srcdir):
            print(
                "Warning: the Firefox checkout directory is currently not in the "
                "Windows Defender exclusion list. This can cause the build process "
                "to be dramatically slowed or broken. To resolve this, follow the "
                "directions here: "
                "https://firefox-source-docs.mozilla.org/setup/windows_build.html"
                "#antivirus-performance",
                file=sys.stderr,
            )

    def install_system_packages(self):
        pass

    def upgrade_mercurial(self, current):
        # Mercurial upstream sometimes doesn't upload wheels, and building
        # from source requires MS Visual C++ 9.0. So we force pip to install
        # the last version that comes with wheels.
        self.pip_install("mercurial", "--only-binary", "mercurial")

    def install_browser_packages(self, mozconfig_builder):
        pass

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        pass

    def install_mobile_android_packages(self, mozconfig_builder):
        self.ensure_mobile_android_packages(mozconfig_builder)

    def install_mobile_android_artifact_mode_packages(self, mozconfig_builder):
        self.ensure_mobile_android_packages(mozconfig_builder, artifact_mode=True)

    def ensure_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        java_bin_dir = self.ensure_java(mozconfig_builder)
        from mach.util import setenv

        setenv("PATH", "{}{}{}".format(java_bin_dir, os.pathsep, os.environ["PATH"]))

        from mozboot import android

        android.ensure_android(
            "windows", artifact_mode=artifact_mode, no_interactive=self.no_interactive
        )

    def generate_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android

        return android.generate_mozconfig("windows", artifact_mode=artifact_mode)

    def generate_mobile_android_artifact_mode_mozconfig(self):
        return self.generate_mobile_android_mozconfig(artifact_mode=True)

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        from mozboot import static_analysis

        self.install_toolchain_static_analysis(
            state_dir, checkout_root, static_analysis.WINDOWS_CLANG_TIDY
        )

    def ensure_sccache_packages(self, state_dir, checkout_root):
        from mozboot import sccache

        self.install_toolchain_artifact(state_dir, checkout_root, sccache.WIN64_SCCACHE)
        self.install_toolchain_artifact(
            state_dir, checkout_root, sccache.RUSTC_DIST_TOOLCHAIN, no_unpack=True
        )
        self.install_toolchain_artifact(
            state_dir, checkout_root, sccache.CLANG_DIST_TOOLCHAIN, no_unpack=True
        )

    def ensure_stylo_packages(self, state_dir, checkout_root):
        # On-device artifact builds are supported; on-device desktop builds are not.
        if is_aarch64_host():
            raise Exception(
                "You should not be performing desktop builds on an "
                "AArch64 device.  If you want to do artifact builds "
                "instead, please choose the appropriate artifact build "
                "option when beginning bootstrap."
            )

        from mozboot import stylo

        self.install_toolchain_artifact(state_dir, checkout_root, stylo.WINDOWS_CLANG)
        self.install_toolchain_artifact(
            state_dir, checkout_root, stylo.WINDOWS_CBINDGEN
        )

    def ensure_nasm_packages(self, state_dir, checkout_root):
        from mozboot import nasm

        self.install_toolchain_artifact(state_dir, checkout_root, nasm.WINDOWS_NASM)

    def ensure_node_packages(self, state_dir, checkout_root):
        from mozboot import node

        # We don't have native aarch64 node available, but aarch64 windows
        # runs x86 binaries, so just use the x86 packages for such hosts.
        node_artifact = node.WIN32 if is_aarch64_host() else node.WIN64
        self.install_toolchain_artifact(state_dir, checkout_root, node_artifact)

    def ensure_dump_syms_packages(self, state_dir, checkout_root):
        from mozboot import dump_syms

        self.install_toolchain_artifact(
            state_dir, checkout_root, dump_syms.WIN64_DUMP_SYMS
        )

    def ensure_fix_stacks_packages(self, state_dir, checkout_root):
        from mozboot import fix_stacks

        self.install_toolchain_artifact(
            state_dir, checkout_root, fix_stacks.WINDOWS_FIX_STACKS
        )

    def ensure_minidump_stackwalk_packages(self, state_dir, checkout_root):
        from mozboot import minidump_stackwalk

        self.install_toolchain_artifact(
            state_dir, checkout_root, minidump_stackwalk.WINDOWS_MINIDUMP_STACKWALK
        )

    def _update_package_manager(self):
        pass

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def pip_install(self, *packages):
        pip_dir = os.path.join(
            os.environ["MOZILLABUILD"], "python", "Scripts", "pip.exe"
        )
        command = [pip_dir, "install", "--upgrade"]
        command.extend(packages)
        self.run(command)
