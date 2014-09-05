#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os

here = os.path.dirname(__file__)
os.path.abspath(sys.path.insert(0, os.path.join(here, "harness")))

from wptrunner import update

if __name__ == "__main__":
    success = update.main()
    sys.exit(0 if success else 1)
