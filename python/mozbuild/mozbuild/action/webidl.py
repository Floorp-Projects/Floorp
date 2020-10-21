# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import sys

from mozwebidlcodegen import BuildSystemWebIDL
from mozbuild.action.util import log_build_task


def main(argv):
    """Perform WebIDL code generation required by the build system."""
    manager = BuildSystemWebIDL.from_environment().manager
    manager.generate_build_files()


if __name__ == "__main__":
    sys.exit(log_build_task(main, sys.argv[1:]))
