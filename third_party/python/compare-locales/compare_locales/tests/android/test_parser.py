# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import unittest

from compare_locales.tests import ParserTestMixin
from compare_locales.parser import (
    Comment,
    Junk,
    Whitespace,
)
from compare_locales.parser.android import DocumentWrapper


class TestAndroidParser(ParserTestMixin, unittest.TestCase):
    maxDiff = None
    filename = 'strings.xml'

    def test_simple_string(self):
        source = '''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- bar -->
  <string name="foo">value</string>
  <!-- bar -->
  <!-- foo -->
  <string name="bar">multi-line comment</string>

  <!-- standalone -->

  <string name="baz">so lonely</string>
</resources>
'''
        self._test(
            source,
            (
                (DocumentWrapper, '<?xml'),
                (DocumentWrapper, '>'),
                (Whitespace, '\n  '),
                ('foo', 'value', 'bar'),
                (Whitespace, '\n'),
                ('bar', 'multi-line comment', 'bar\nfoo'),
                (Whitespace, '\n  '),
                (Comment, 'standalone'),
                (Whitespace, '\n  '),
                ('baz', 'so lonely'),
                (Whitespace, '\n'),
                (DocumentWrapper, '</resources>')
            )
        )

    def test_bad_doc(self):
        source = '''\
<?xml version="1.0" ?>
<not-a-resource/>
'''
        self._test(
            source,
            (
                (Junk, '<not-a-resource/>'),
            )
        )

    def test_bad_elements(self):
        source = '''\
<?xml version="1.0" ?>
<resources>
  <string name="first">value</string>
  <non-string name="bad">value</non-string>
  <string name="mid">value</string>
  <string nomine="dom">value</string>
  <string name="last">value</string>
</resources>
'''
        self._test(
            source,
            (
                (DocumentWrapper, '<?xml'),
                (DocumentWrapper, '>'),
                (Whitespace, '\n  '),
                ('first', 'value'),
                (Whitespace, '\n  '),
                (Junk, '<non-string name="bad">'),
                (Whitespace, '\n  '),
                ('mid', 'value'),
                (Whitespace, '\n  '),
                (Junk, '<string nomine="dom">'),
                (Whitespace, '\n  '),
                ('last', 'value'),
                (Whitespace, '\n'),
                (DocumentWrapper, '</resources>')
            )
        )

    def test_xml_parse_error(self):
        source = 'no xml'
        self._test(
            source,
            (
                (Junk, 'no xml'),
            )
        )

    def test_empty_strings(self):
        source = '''\
<?xml version="1.0" ?>
<resources>
  <string name="one"></string>
  <string name="two"/>
</resources>
'''
        self._test(
            source,
            (
                (DocumentWrapper, '<?xml'),
                (DocumentWrapper, '>'),
                (Whitespace, '\n  '),
                ('one', ''),
                (Whitespace, '\n  '),
                ('two', ''),
                (Whitespace, '\n'),
                (DocumentWrapper, '</resources>')
            )
        )
