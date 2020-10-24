# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os


def normsep(path):
    '''
    Normalize path separators, by using forward slashes instead of whatever
    :py:const:`os.sep` is.
    '''
    if os.sep != '/':
        # Python 2 is happy to do things like byte_string.replace(u'foo',
        # u'bar'), but not Python 3.
        if isinstance(path, bytes):
            path = path.replace(os.sep.encode('ascii'), b'/')
        else:
            path = path.replace(os.sep, '/')
    if os.altsep and os.altsep != '/':
        if isinstance(path, bytes):
            path = path.replace(os.altsep.encode('ascii'), b'/')
        else:
            path = path.replace(os.altsep, '/')
    return path
