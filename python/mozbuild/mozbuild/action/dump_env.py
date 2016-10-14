# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We invoke a Python program to dump our environment in order to get
# native paths printed on Windows so that these paths can be incorporated
# into Python configure's environment.
import os
for key, value in os.environ.items():
    print('%s=%s' % (key, value))
