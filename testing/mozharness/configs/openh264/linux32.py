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
    "exes": {
        "gittool.py": [os.path.join(external_tools_path, "gittool.py")],
        "python2.7": "python2.7",
    },
    "dump_syms_binary": "{}/dump_syms/dump_syms".format(os.environ["MOZ_FETCHES_DIR"]),
    "arch": "x86",
    "avoid_avx2": True,
    "operating_system": "linux",
    "partial_env": {
        "PATH": (
            "{MOZ_FETCHES_DIR}/clang/bin:"
            "{MOZ_FETCHES_DIR}/binutils/bin:"
            "{MOZ_FETCHES_DIR}/nasm:%(PATH)s".format(
                MOZ_FETCHES_DIR=os.environ["MOZ_FETCHES_DIR"]
            )
        ),
    },
}
