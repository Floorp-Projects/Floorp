# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import os

import mozharness

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    "external_tools",
)

config = {
    "tooltool_manifest_file": "osx.manifest",
    "tooltool_cache": "/builds/tooltool_cache",
    "exes": {
        "gittool.py": [os.path.join(external_tools_path, "gittool.py")],
        "python2.7": "python2.7",
    },
    "dump_syms_binary": "{}/dump_syms/dump_syms".format(os.environ["MOZ_FETCHES_DIR"]),
    "arch": "x64",
    "use_yasm": True,
    "operating_system": "darwin",
    "partial_env": {
        "CXXFLAGS": (
            "-target x86_64-apple-darwin "
            "-isysroot %(abs_work_dir)s/MacOSX10.11.sdk "
            "-mmacosx-version-min=10.11"
        ),
        "LDFLAGS": (
            "-target x86_64-apple-darwin "
            "-isysroot %(abs_work_dir)s/MacOSX10.11.sdk "
            "-mmacosx-version-min=10.11"
        ),
        "PATH": (
            "{MOZ_FETCHES_DIR}/clang/bin/:{MOZ_FETCHES_DIR}/cctools/bin/:%(PATH)s".format(
                MOZ_FETCHES_DIR=os.environ["MOZ_FETCHES_DIR"]
            )
        ),
    },
}
