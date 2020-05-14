# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import unittest

from compare_locales.checks.base import CSSCheckMixin


class CSSParserTest(unittest.TestCase):
    def setUp(self):
        self.mixin = CSSCheckMixin()

    def test_other(self):
        refMap, errors = self.mixin.parse_css_spec('foo')
        self.assertIsNone(refMap)
        self.assertIsNone(errors)

    def test_css_specs(self):
        for prop in (
            'min-width', 'width', 'max-width',
            'min-height', 'height', 'max-height',
        ):
            refMap, errors = self.mixin.parse_css_spec('{}:1px;'.format(prop))
            self.assertDictEqual(
                refMap, {prop: 'px'}
            )
            self.assertIsNone(errors)

    def test_single_whitespace(self):
        refMap, errors = self.mixin.parse_css_spec('width:15px;')
        self.assertDictEqual(
            refMap, {'width': 'px'}
        )
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec('width   : \t 15px  ;  ')
        self.assertDictEqual(
            refMap, {'width': 'px'}
        )
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec('width: 15px')
        self.assertDictEqual(
            refMap, {'width': 'px'}
        )
        self.assertIsNone(errors)

    def test_multiple(self):
        refMap, errors = self.mixin.parse_css_spec('width:15px;height:20.2em;')
        self.assertDictEqual(
            refMap, {'height': 'em', 'width': 'px'}
        )
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec(
            'width:15px \t\t;  height:20em'
        )
        self.assertDictEqual(
            refMap, {'height': 'em', 'width': 'px'}
        )
        self.assertIsNone(errors)

    def test_errors(self):
        refMap, errors = self.mixin.parse_css_spec('width:15pxfoo')
        self.assertDictEqual(
            refMap, {'width': 'px'}
        )
        self.assertListEqual(
            errors, [{'pos': 10, 'code': 'css-bad-content'}]
        )
        refMap, errors = self.mixin.parse_css_spec('width:15px height:20em')
        self.assertDictEqual(
            refMap, {'height': 'em', 'width': 'px'}
        )
        self.assertListEqual(
            errors, [{'pos': 10, 'code': 'css-missing-semicolon'}]
        )
        refMap, errors = self.mixin.parse_css_spec('witdth:15px')
        self.assertIsNone(refMap)
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec('width:1,5px')
        self.assertIsNone(refMap)
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec('width:1.5.1px')
        self.assertIsNone(refMap)
        self.assertIsNone(errors)
        refMap, errors = self.mixin.parse_css_spec('width:1.px')
        self.assertIsNone(refMap)
        self.assertIsNone(errors)
