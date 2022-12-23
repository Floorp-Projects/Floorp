# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# We invoke a Python program to dump our environment in order to get
# native paths printed on Windows so that these paths can be incorporated
# into Python configure's environment.
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))

from shellutil import quote


def environ():
    # We would use six.ensure_text but the global Python isn't guaranteed to have
    # the correct version of six installed.
    def ensure_text(s):
        if sys.version_info > (3, 0) or isinstance(s, unicode):
            # os.environ always returns string keys and values in Python 3.
            return s
        else:
            return s.decode("utf-8")

    return [(ensure_text(k), ensure_text(v)) for (k, v) in os.environ.items()]


for key, value in environ():
    print("%s=%s" % (key, quote(value)))
