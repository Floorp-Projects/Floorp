# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import itertools
from distutils.version import LooseVersion


class Version(LooseVersion):
    '''A simple subclass of distutils.version.LooseVersion.
    Adds attributes for `major`, `minor`, `patch` for the first three
    version components so users can easily pull out major/minor
    versions, like:

    v = Version('1.2b')
    v.major == 1
    v.minor == 2
    v.patch == 0
    '''
    def __init__(self, version):
        # Can't use super, LooseVersion's base class is not a new-style class.
        LooseVersion.__init__(self, version)
        # Take the first three integer components, stopping at the first
        # non-integer and padding the rest with zeroes.
        (self.major, self.minor, self.patch) = list(itertools.chain(
            itertools.takewhile(lambda x:isinstance(x, int), self.version),
            (0, 0, 0)))[:3]


    def __cmp__(self, other):
        # LooseVersion checks isinstance(StringType), so work around it.
        if isinstance(other, unicode):
            other = other.encode('ascii')
        return LooseVersion.__cmp__(self, other)
