# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import re
import six


class StringVersion(six.text_type):
    """
    A string version that can be compared with comparison operators.
    """

    # Pick out numeric and non-numeric parts (a match group for each type).
    pat = re.compile(r'(\d+)|([^\d.]+)')

    def __init__(self, vstring):
        super(StringVersion, self).__init__()

        # We'll use unicode internally.
        # This check is mainly for python2 strings (which are bytes).
        if isinstance(vstring, bytes):
            vstring = vstring.decode("ascii")

        self.vstring = vstring

        # Store parts as strings to ease comparisons.
        self.version = []
        parts = self.pat.findall(vstring)
        # Pad numeric parts with leading zeros for ordering.
        for i, obj in enumerate(parts):
            if obj[0]:
                self.version.append(obj[0].zfill(8))
            else:
                self.version.append(obj[1])

    def __str__(self):
        return self.vstring

    def __repr__(self):
        return "StringVersion ('%s')" % str(self)

    def _cmp(self, other):
        if not isinstance(other, StringVersion):
            other = StringVersion(other)

        if self.version == other.version:
            return 0
        if self.version < other.version:
            return -1
        if self.version > other.version:
            return 1

    # operator overloads
    def __eq__(self, other):
        return self._cmp(other) == 0

    def __lt__(self, other):
        return self._cmp(other) < 0

    def __le__(self, other):
        return self._cmp(other) <= 0

    def __gt__(self, other):
        return self._cmp(other) > 0

    def __ge__(self, other):
        return self._cmp(other) >= 0
