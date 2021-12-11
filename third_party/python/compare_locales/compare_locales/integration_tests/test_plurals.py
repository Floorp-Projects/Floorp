# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import unittest
from six.moves.urllib.error import URLError
from six.moves.urllib.request import urlopen

from compare_locales import plurals


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
        # Strip matches from dicts, to make diff for test small
        locales = list(reference_form_map)
        cl_form_map = {}
        for locale in locales:
            cl_form = str(plurals.get_plural_rule(locale))
            if cl_form == reference_form_map[locale]:
                reference_form_map.pop(locale)
            else:
                cl_form_map[locale] = cl_form
        self.assertDictEqual(reference_form_map, cl_form_map)

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
