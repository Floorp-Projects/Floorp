# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from mach.site import PythonVirtualenv
from mach.util import get_state_dir


MINIMUM_RUST_VERSION = "1.53.0"


def get_tools_dir(srcdir=False):
    if os.environ.get("MOZ_AUTOMATION") and "MOZ_FETCHES_DIR" in os.environ:
        return os.environ["MOZ_FETCHES_DIR"]
    return get_state_dir(srcdir)


def get_mach_virtualenv_root():
    return os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "_virtualenvs", "mach"
    )


def get_mach_virtualenv_binary():
    root = get_mach_virtualenv_root()
    return PythonVirtualenv(root).python_path
