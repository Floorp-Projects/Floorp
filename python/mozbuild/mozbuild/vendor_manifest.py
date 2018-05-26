# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys

from . import moz_yaml


def verify_manifests(files):
    success = True
    for fn in files:
        try:
            moz_yaml.load_moz_yaml(fn)
            print('%s: OK' % fn)
        except moz_yaml.VerifyError as e:
            success = False
            print(e)
    sys.exit(0 if success else 1)
