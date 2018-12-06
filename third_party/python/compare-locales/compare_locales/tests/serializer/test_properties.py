# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.serializer import serialize
from . import Helper


class TestPropertiesSerializer(Helper, unittest.TestCase):
    name = 'foo.properties'
    reference_content = """\
this = is English

# another one bites
another = message
"""

    def test_nothing_new_or_old(self):
        output = serialize(self.name, self.reference, [], {})
        self.assertMultiLineEqual(output.decode(self.parser.encoding), '\n\n')

    def test_obsolete_old_string(self):
        self._test(
            """\
# we used to have this
old = stuff with comment
""",
            {},
            """\


""")

    def test_nothing_old_new_translation(self):
        self._test(
            "",
            {
                "another": "localized message"
            },
            """\


# another one bites
another = localized message
"""
        )

    def test_old_message_new_other_translation(self):
        self._test(
            """\
this = is localized
""",
            {
                "another": "localized message"
            },
            """\
this = is localized

# another one bites
another = localized message
"""
        )

    def test_old_message_new_same_translation(self):
        self._test(
            """\
this = is localized
""",
            {
                "this": "has a better message"
            },
            """\
this = has a better message

"""
        )


class TestPropertiesDuplicateComment(Helper, unittest.TestCase):
    name = 'foo.properties'
    reference_content = """\
# repetitive
one = one
# repetitive
two = two
"""

    def test_missing_translation(self):
        self._test(
            """\
# repetitive

# repetitive
two = two
""",
            {},
            """\
# repetitive

# repetitive
two = two
"""
        )
