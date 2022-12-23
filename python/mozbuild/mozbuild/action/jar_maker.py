# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

import mozbuild.jar
from mozbuild.action.util import log_build_task


def main(args):
    return mozbuild.jar.main(args)


if __name__ == "__main__":
    sys.exit(log_build_task(main, sys.argv[1:]))
