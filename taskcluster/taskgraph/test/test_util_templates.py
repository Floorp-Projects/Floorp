# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest
import mozunit
import textwrap
from taskgraph.util.templates import (
    Templates,
    TemplatesException
)

files = {}
files['/fixtures/circular.yml'] = textwrap.dedent("""\
    $inherits:
      from: 'circular_ref.yml'
      variables:
        woot: 'inherit'
    """)

files['/fixtures/inherit.yml'] = textwrap.dedent("""\
    $inherits:
      from: 'templates.yml'
      variables:
        woot: 'inherit'
    """)

files['/fixtures/extend_child.yml'] = textwrap.dedent("""\
    list: ['1', '2', '3']
    was_list: ['1']
    obj:
      level: 1
      deeper:
        woot: 'bar'
        list: ['baz']
    """)

files['/fixtures/circular_ref.yml'] = textwrap.dedent("""\
    $inherits:
      from: 'circular.yml'
    """)

files['/fixtures/child_pass.yml'] = textwrap.dedent("""\
    values:
      - {{a}}
      - {{b}}
      - {{c}}
    """)

files['/fixtures/inherit_pass.yml'] = textwrap.dedent("""\
    $inherits:
      from: 'child_pass.yml'
      variables:
        a: 'a'
        b: 'b'
        c: 'c'
    """)

files['/fixtures/deep/2.yml'] = textwrap.dedent("""\
    $inherits:
      from: deep/1.yml

    """)

files['/fixtures/deep/3.yml'] = textwrap.dedent("""\
    $inherits:
      from: deep/2.yml

    """)

files['/fixtures/deep/4.yml'] = textwrap.dedent("""\
    $inherits:
      from: deep/3.yml
    """)

files['/fixtures/deep/1.yml'] = textwrap.dedent("""\
    variable: {{value}}
    """)

files['/fixtures/simple.yml'] = textwrap.dedent("""\
    is_simple: true
    """)

files['/fixtures/templates.yml'] = textwrap.dedent("""\
    content: 'content'
    variable: '{{woot}}'
    """)

files['/fixtures/extend_parent.yml'] = textwrap.dedent("""\
    $inherits:
      from: 'extend_child.yml'

    list: ['4']
    was_list:
      replaced: true
    obj:
      level: 2
      from_parent: true
      deeper:
        list: ['bar']
    """)


class TemplatesTest(unittest.TestCase):

    def setUp(self):
        self.mocked_open = mozunit.MockedOpen(files)
        self.mocked_open.__enter__()
        self.subject = Templates('/fixtures')

    def tearDown(self):
        self.mocked_open.__exit__(None, None, None)

    def test_invalid_path(self):
        with self.assertRaisesRegexp(TemplatesException, 'must be a directory'):
            Templates('/zomg/not/a/dir')

    def test_no_templates(self):
        content = self.subject.load('simple.yml', {})
        self.assertEquals(content, {
            'is_simple': True
        })

    def test_with_templates(self):
        content = self.subject.load('templates.yml', {
            'woot': 'bar'
        })

        self.assertEquals(content, {
            'content': 'content',
            'variable': 'bar'
        })

    def test_inheritance(self):
        '''
        The simple single pass inheritance case.
        '''
        content = self.subject.load('inherit.yml', {})
        self.assertEqual(content, {
            'content': 'content',
            'variable': 'inherit'
        })

    def test_inheritance_implicat_pass(self):
        '''
        Implicitly pass parameters from the child to the ancestor.
        '''
        content = self.subject.load('inherit_pass.yml', {
            'a': 'overriden'
        })

        self.assertEqual(content, {'values': ['overriden', 'b', 'c']})

    def test_inheritance_circular(self):
        '''
        Circular reference handling.
        '''
        with self.assertRaisesRegexp(TemplatesException, 'circular'):
            self.subject.load('circular.yml', {})

    def test_deep_inheritance(self):
        content = self.subject.load('deep/4.yml', {
            'value': 'myvalue'
        })
        self.assertEqual(content, {'variable': 'myvalue'})

    def test_inheritance_with_simple_extensions(self):
        content = self.subject.load('extend_parent.yml', {})
        self.assertEquals(content, {
            'list': ['1', '2', '3', '4'],
            'obj': {
                'from_parent': True,
                'deeper': {
                    'woot': 'bar',
                    'list': ['baz', 'bar']
                },
                'level': 2,
            },
            'was_list': {'replaced': True}
        })


if __name__ == '__main__':
    mozunit.main()
