# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from distutils.version import LooseVersion


class StringVersion(str):
    """
    A string version that can be compared with comparison operators.
    """

    def __init__(self, vstring):
        str.__init__(self, vstring)
        self.version = LooseVersion(vstring)

    def __repr__(self):
        return "StringVersion ('%s')" % self

    def __to_version(self, other):
        if not isinstance(other, StringVersion):
            other = StringVersion(other)
        return other.version

    # rich comparison methods

    def __lt__(self, other):
        return self.version < self.__to_version(other)

    def __le__(self, other):
        return self.version <= self.__to_version(other)

    def __eq__(self, other):
        return self.version == self.__to_version(other)

    def __ne__(self, other):
        return self.version != self.__to_version(other)

    def __gt__(self, other):
        return self.version > self.__to_version(other)

    def __ge__(self, other):
        return self.version >= self.__to_version(other)
