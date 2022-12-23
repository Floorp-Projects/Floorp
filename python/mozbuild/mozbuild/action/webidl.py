# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from mozwebidlcodegen import create_build_system_manager

from mozbuild.action.util import log_build_task


def main(argv):
    """Perform WebIDL code generation required by the build system."""
    manager = create_build_system_manager()
    manager.generate_build_files()


if __name__ == "__main__":
    sys.exit(log_build_task(main, sys.argv[1:]))
