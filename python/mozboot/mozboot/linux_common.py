# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# An easy way for distribution-specific bootstrappers to share the code
# needed to install Stylo and Node dependencies.  This class must come before
# BaseBootstrapper in the inheritance list.

from __future__ import absolute_import, print_function

import os


def is_non_x86_64():
    return os.uname()[4] != 'x86_64'


class SccacheInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_sccache_packages(self, state_dir, checkout_root):
        from mozboot import sccache

        self.install_toolchain_artifact(state_dir, checkout_root, sccache.LINUX_SCCACHE)


class StyloInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_stylo_packages(self, state_dir, checkout_root):
        from mozboot import stylo

        if is_non_x86_64():
            print('Cannot install bindgen clang and cbindgen packages from taskcluster.\n'
                  'Please install these packages manually.')
            return

        self.install_toolchain_artifact(state_dir, checkout_root, stylo.LINUX_CLANG)
        self.install_toolchain_artifact(state_dir, checkout_root, stylo.LINUX_CBINDGEN)


class NasmInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_nasm_packages(self, state_dir, checkout_root):
        if is_non_x86_64():
            print('Cannot install nasm from taskcluster.\n'
                  'Please install this package manually.')
            return

        from mozboot import nasm
        self.install_toolchain_artifact(state_dir, checkout_root, nasm.LINUX_NASM)


class NodeInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_node_packages(self, state_dir, checkout_root):
        if is_non_x86_64():
            print('Cannot install node package from taskcluster.\n'
                  'Please install this package manually.')
            return

        from mozboot import node
        self.install_toolchain_artifact(state_dir, checkout_root, node.LINUX)


class ClangStaticAnalysisInstall(object):
    def __init__(self, **kwargs):
        pass

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        if is_non_x86_64():
            print('Cannot install static analysis tools from taskcluster.\n'
                  'Please install these tools manually.')
            return

        from mozboot import static_analysis
        self.install_toolchain_static_analysis(
            state_dir, checkout_root, static_analysis.LINUX_CLANG_TIDY)
