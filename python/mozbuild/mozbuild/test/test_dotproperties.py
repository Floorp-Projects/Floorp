# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import sys
import unittest

from StringIO import StringIO

from mozbuild.dotproperties import (
    DotProperties,
)

from mozunit import (
    main,
)

if sys.version_info[0] == 3:
    str_type = 'str'
else:
    str_type = 'unicode'


class TestDotProperties(unittest.TestCase):
    def test_get(self):
        contents = StringIO('''
key=value
''')
        p = DotProperties(contents)
        self.assertEqual(p.get('missing'), None)
        self.assertEqual(p.get('missing', 'default'), 'default')
        self.assertEqual(p.get('key'), 'value')


    def test_update(self):
        contents = StringIO('''
old=old value
key=value
''')
        p = DotProperties(contents)
        self.assertEqual(p.get('old'), 'old value')
        self.assertEqual(p.get('key'), 'value')

        new_contents = StringIO('''
key=new value
''')
        p.update(new_contents)
        self.assertEqual(p.get('old'), 'old value')
        self.assertEqual(p.get('key'), 'new value')


    def test_get_list(self):
        contents = StringIO('''
list.0=A
list.1=B
list.2=C

order.1=B
order.0=A
order.2=C
''')
        p = DotProperties(contents)
        self.assertEqual(p.get_list('missing'), [])
        self.assertEqual(p.get_list('list'), ['A', 'B', 'C'])
        self.assertEqual(p.get_list('order'), ['A', 'B', 'C'])


    def test_get_dict(self):
        contents = StringIO('''
A.title=title A

B.title=title B
B.url=url B
''')
        p = DotProperties(contents)
        self.assertEqual(p.get_dict('missing'), {})
        self.assertEqual(p.get_dict('A'), {'title': 'title A'})
        self.assertEqual(p.get_dict('B'), {'title': 'title B', 'url': 'url B'})
        with self.assertRaises(ValueError):
            p.get_dict('A', required_keys=['title', 'url'])
        with self.assertRaises(ValueError):
            p.get_dict('missing', required_keys=['key'])


if __name__ == '__main__':
    main()
