# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.frontend.sandbox_symbols import (
    FUNCTIONS,
    SPECIAL_VARIABLES,
    VARIABLES,
)


class TestSymbols(unittest.TestCase):
    def _verify_doc(self, doc):
        # Documentation should be of the format:
        # """SUMMARY LINE
        #
        # EXTRA PARAGRAPHS
        # """

        self.assertNotIn('\r', doc)

        lines = doc.split('\n')

        # No trailing whitespace.
        for line in lines[0:-1]:
            self.assertEqual(line, line.rstrip())

        self.assertGreater(len(lines), 0)
        self.assertGreater(len(lines[0].strip()), 0)

        # Last line should be empty.
        self.assertEqual(lines[-1].strip(), '')

    def test_documentation_formatting(self):
        for typ, inp, default, doc, tier in VARIABLES.values():
            self._verify_doc(doc)

        for attr, args, doc in FUNCTIONS.values():
            self._verify_doc(doc)

        for typ, doc in SPECIAL_VARIABLES.values():
            self._verify_doc(doc)


if __name__ == '__main__':
    main()
