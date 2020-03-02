# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from glean_parser import lint
from pathlib import Path


def main(output, *filenames):
    if lint.glinter([Path(x) for x in filenames], {"allow_reserved": False}):
        sys.exit(1)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
