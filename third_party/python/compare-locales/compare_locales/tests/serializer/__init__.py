# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from compare_locales.parser import getParser
from compare_locales.serializer import serialize


class Helper(object):
    """Mixin to test serializers.

    Reads the reference_content into self.reference, and uses
    that to serialize in _test.
    """
    name = None
    reference_content = None

    def setUp(self):
        p = self.parser = getParser(self.name)
        p.readUnicode(self.reference_content)
        self.reference = list(p.walk())

    def _test(self, old_content, new_data, expected):
        """Test with old content, new data, and the reference data
        against the expected unicode output.
        """
        self.parser.readUnicode(old_content)
        old_l10n = list(self.parser.walk())
        output = serialize(self.name, self.reference, old_l10n, new_data)
        self.assertMultiLineEqual(
            output.decode(self.parser.encoding),
            expected
        )
