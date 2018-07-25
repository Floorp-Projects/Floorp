# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ast
import json
import os
import unittest
import six
from six.moves.urllib.error import URLError
from six.moves.urllib.request import urlopen


TRANSVISION_URL = (
    'https://transvision.mozfr.org/'
    'api/v1/entity/gecko_strings/'
    '?id=toolkit/chrome/global/intl.properties:pluralRule'
)


class TestPlural(unittest.TestCase):
    '''Integration test for plural forms and l10n-central.

    Having more plural forms than in l10n-central is OK, missing or
    mismatching ones isn't.
    Depends on Transvision.
    '''
    maxDiff = None

    def test_valid_forms(self):
        reference_form_map = self._load_transvision()
        compare_locales_map = self._parse_plurals_py()
        # Notify locales in compare-locales but not in Transvision
        # Might be incubator locales
        extra_locales = set()
        extra_locales.update(compare_locales_map)
        extra_locales.difference_update(reference_form_map)
        for locale in sorted(extra_locales):
            print("{} not in Transvision, OK".format(locale))
            compare_locales_map.pop(locale)
        # Strip matches from dicts, to make diff for test small
        locales = set()
        locales.update(compare_locales_map)
        locales.intersection_update(reference_form_map)
        for locale in locales:
            if compare_locales_map[locale] == reference_form_map[locale]:
                compare_locales_map.pop(locale)
                reference_form_map.pop(locale)
        self.assertDictEqual(reference_form_map, compare_locales_map)

    def _load_transvision(self):
        '''Use the Transvision API to load all values of pluralRule
        in intl.properties.
        Skip test on load failure.
        '''
        try:
            data = urlopen(TRANSVISION_URL).read()
        except URLError:
            raise unittest.SkipTest("Couldn't load Transvision API.")
        return json.loads(data)

    def _parse_plurals_py(self):
        '''Load compare_locales.plurals, parse the AST, and inspect
        the dictionary assigned to CATEGORIES_BY_LOCALE to find
        the actual plural number.
        Convert both number and locale code to unicode for comparing
        to json.
        '''
        path = os.path.join(os.path.dirname(__file__), '..', 'plurals.py')
        with open(path) as source_file:
            plurals_ast = ast.parse(source_file.read())
        assign_cats_statement = [
            s for s in plurals_ast.body
            if isinstance(s, ast.Assign)
            and any(t.id == 'CATEGORIES_BY_LOCALE' for t in s.targets)
        ][0]
        return dict(
            (six.text_type(k.s), six.text_type(v.slice.value.n))
            for k, v in zip(
                assign_cats_statement.value.keys,
                assign_cats_statement.value.values
            )
        )
