# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# An easy way for distribution-specific bootstrappers to share the code
# needed to install Stylo and Node dependencies.  This class must come before
# BaseBootstrapper in the inheritance list.

from __future__ import absolute_import, print_function, unicode_literals

import platform


def is_non_x86_64():
    return platform.machine() != "x86_64"


class SccacheInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_sccache_packages(self, state_dir, checkout_root):
        from mozboot import sccache

        self.install_toolchain_artifact(state_dir, checkout_root, sccache.LINUX_SCCACHE)


class FixStacksInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_fix_stacks_packages(self, state_dir, checkout_root):
        from mozboot import fix_stacks

        self.install_toolchain_artifact(
            state_dir, checkout_root, fix_stacks.LINUX_FIX_STACKS
        )


class StyloInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_stylo_packages(self, state_dir, checkout_root):
        from mozboot import stylo

        if is_non_x86_64():
            print(
                "Cannot install bindgen clang and cbindgen packages from taskcluster.\n"
                "Please install these packages manually."
            )
            return

        self.install_toolchain_artifact(state_dir, checkout_root, stylo.LINUX_CLANG)
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.LINUX_CBINDGEN)


class NasmInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_nasm_packages(self, state_dir, checkout_root):
        if is_non_x86_64():
            print(
                "Cannot install nasm from taskcluster.\n"
                "Please install this package manually."
            )
            return

        from mozboot import nasm

        self.install_toolchain_artifact(state_dir, checkout_root, nasm.LINUX_NASM)


class NodeInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_node_packages(self, state_dir, checkout_root):
        if is_non_x86_64():
            print(
                "Cannot install node package from taskcluster.\n"
                "Please install this package manually."
            )
            return

        from mozboot import node

        self.install_toolchain_artifact(state_dir, checkout_root, node.LINUX)


class ClangStaticAnalysisInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        if is_non_x86_64():
            print(
                "Cannot install static analysis tools from taskcluster.\n"
                "Please install these tools manually."
            )
            return

        from mozboot import static_analysis

        self.install_toolchain_static_analysis(
            state_dir, checkout_root, static_analysis.LINUX_CLANG_TIDY
        )


class MinidumpStackwalkInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_minidump_stackwalk_packages(self, state_dir, checkout_root):
        from mozboot import minidump_stackwalk

        self.install_toolchain_artifact(
            state_dir, checkout_root, minidump_stackwalk.LINUX_MINIDUMP_STACKWALK
        )


class MobileAndroidBootstrapper(object):
    def __init__(self, **kwargs):
        pass

    def install_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        from mozboot import android

        os_arch = platform.machine()
        android.ensure_android(
            "linux",
            os_arch,
            artifact_mode=artifact_mode,
            no_interactive=self.no_interactive,
        )
        android.ensure_android(
            "linux",
            os_arch,
            artifact_mode=artifact_mode,
            no_interactive=self.no_interactive,
            system_images_only=True,
            avd_manifest_path=android.AVD_MANIFEST_X86_64,
        )
        android.ensure_android(
            "linux",
            os_arch,
            artifact_mode=artifact_mode,
            no_interactive=self.no_interactive,
            system_images_only=True,
            avd_manifest_path=android.AVD_MANIFEST_ARM,
        )

    def install_mobile_android_artifact_mode_packages(self, mozconfig_builder):
        self.install_mobile_android_packages(mozconfig_builder, artifact_mode=True)

    def ensure_mobile_android_packages(self, state_dir, checkout_root):
        from mozboot import android

        self.install_toolchain_artifact(
            state_dir, checkout_root, android.LINUX_X86_64_ANDROID_AVD
        )
        self.install_toolchain_artifact(
            state_dir, checkout_root, android.LINUX_ARM_ANDROID_AVD
        )

    def generate_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android

        return android.generate_mozconfig("linux", artifact_mode=artifact_mode)

    def generate_mobile_android_artifact_mode_mozconfig(self):
        return self.generate_mobile_android_mozconfig(artifact_mode=True)


class LinuxBootstrapper(
    ClangStaticAnalysisInstall,
    FixStacksInstall,
    MinidumpStackwalkInstall,
    MobileAndroidBootstrapper,
    NasmInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
):
    def __init__(self, **kwargs):
        pass
