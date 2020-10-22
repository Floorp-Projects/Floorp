# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import sys


def main(output, *locales):
    locales = list(locales)
    if "en-US" not in locales:
        locales.append("en-US")

    print(",".join(locales), file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
