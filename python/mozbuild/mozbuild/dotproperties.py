# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains utility functions for reading .properties files, like
# region.properties.

from __future__ import absolute_import, unicode_literals

import codecs
import re
import sys

if sys.version_info[0] == 3:
    str_type = str
else:
    str_type = basestring

class DotProperties:
    r'''A thin representation of a key=value .properties file.'''

    def __init__(self, file=None):
        self._properties = {}
        if file:
            self.update(file)

    def update(self, file):
        '''Updates properties from a file name or file-like object.

        Ignores empty lines and comment lines.'''

        if isinstance(file, str_type):
            f = codecs.open(file, 'r', 'utf-8')
        else:
            f = file

        for l in f.readlines():
            line = l.strip()
            if not line or line.startswith('#'):
                continue
            (k, v) = re.split('\s*=\s*', line, 1)
            self._properties[k] = v

    def get(self, key, default=None):
        return self._properties.get(key, default)

    def get_list(self, prefix):
        '''Turns {'list.0':'foo', 'list.1':'bar'} into ['foo', 'bar'].

        Returns [] to indicate an empty or missing list.'''

        if not prefix.endswith('.'):
            prefix = prefix + '.'
        indexes = []
        for k, v in self._properties.iteritems():
            if not k.startswith(prefix):
                continue
            key = k[len(prefix):]
            if '.' in key:
                # We have something like list.sublist.0.
                continue
            indexes.append(int(key))
        return [self._properties[prefix + str(index)] for index in sorted(indexes)]

    def get_dict(self, prefix, required_keys=[]):
        '''Turns {'foo.title':'title', ...} into {'title':'title', ...}.

        If |required_keys| is present, it must be an iterable of required key
        names.  If a required key is not present, ValueError is thrown.

        Returns {} to indicate an empty or missing dict.'''

        if not prefix.endswith('.'):
            prefix = prefix + '.'

        D = dict((k[len(prefix):], v) for k, v in self._properties.iteritems()
                 if k.startswith(prefix) and '.' not in k[len(prefix):])

        for required_key in required_keys:
            if not required_key in D:
                raise ValueError('Required key %s not present' % required_key)

        return D
