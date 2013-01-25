# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
from mozpack.packager.unpack import unpack

def main():
    if len(sys.argv) != 2:
        print >>sys.stderr, "Usage: %s directory" % \
                            os.path.basename(sys.argv[0])
        sys.exit(1)

    unpack(sys.argv[1])

if __name__ == "__main__":
    main()
