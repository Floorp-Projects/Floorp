# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

LINUX_SCCACHE = 'linux64-sccache'
MACOS_SCCACHE = 'macosx64-sccache'
WIN64_SCCACHE = 'win64-sccache'

# sccache-dist currently expects clients to provide toolchains when
# distributing from macOS or Windows, so we download linux binaries capable
# of cross-compiling for these cases.
RUSTC_DIST_TOOLCHAIN = 'rustc-dist-toolchain'
CLANG_DIST_TOOLCHAIN = 'clang-dist-toolchain'
