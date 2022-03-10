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

    def ensure_sccache_packages(self):
        self.install_toolchain_artifact("sccache")


class FixStacksInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_fix_stacks_packages(self):
        self.install_toolchain_artifact("fix-stacks")


class StyloInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_stylo_packages(self):
        if is_non_x86_64():
            print(
                "Cannot install bindgen clang and cbindgen packages from taskcluster.\n"
                "Please install these packages manually."
            )
            return

        self.install_toolchain_artifact("clang")
        self.install_toolchain_artifact("cbindgen")


class NasmInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_nasm_packages(self):
        if is_non_x86_64():
            print(
                "Cannot install nasm from taskcluster.\n"
                "Please install this package manually."
            )
            return

        self.install_toolchain_artifact("nasm")


class NodeInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_node_packages(self):
        if is_non_x86_64():
            print(
                "Cannot install node package from taskcluster.\n"
                "Please install this package manually."
            )
            return

        self.install_toolchain_artifact("node")


class ClangStaticAnalysisInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_clang_static_analysis_package(self):
        if is_non_x86_64():
            print(
                "Cannot install static analysis tools from taskcluster.\n"
                "Please install these tools manually."
            )
            return

        from mozboot import static_analysis

        self.install_toolchain_static_analysis(static_analysis.LINUX_CLANG_TIDY)


class MinidumpStackwalkInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_minidump_stackwalk_packages(self):
        self.install_toolchain_artifact("minidump-stackwalk")


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

    def ensure_mobile_android_packages(self):
        from mozboot import android

        android.ensure_java("linux", platform.machine())
        self.install_toolchain_artifact(android.LINUX_X86_64_ANDROID_AVD)
        self.install_toolchain_artifact(android.LINUX_ARM_ANDROID_AVD)

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
