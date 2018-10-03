# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This action is used to generate the wpt manifest

import os
import sys

import buildconfig


def main():
    print("Downloading wpt manifest")
    sys.path.insert(0, buildconfig.topsrcdir)
    import manifestupdate
    return 0 if manifestupdate.run(buildconfig.topsrcdir, buildconfig.topobjdir) else 1


if __name__ == '__main__':
    sys.exit(main())
