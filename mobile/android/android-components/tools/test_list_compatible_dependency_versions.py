#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import list_compatible_dependency_versions as module
import os
import unittest

SCRIPT_DIR=os.path.dirname(os.path.realpath(__file__))
AC_ROOT=os.path.join(SCRIPT_DIR, '..')

class AssumptionsTestCase(unittest.TestCase):
    """A class to test that some assumptions we've made in the code,
    including file formats and paths, are upheld.
    """

    def testAcCheckoutFileFormatAndPathCorrect(self):
        # If file format or path is incorrect, this method will raise.
        module.ac_checkout_to_gv_version(AC_ROOT)

    def testFenixCheckoutCorrectParse(self):
        # Unfortunately, we'd have to guess where the fenix install is
        # and this might break on different machines.
        pass

    def testPomFormatCorrect(self):
        # We could check the pom file online or from a local m2 but
        # it seems not worth it.
        pass

if __name__ == '__main__':
    unittest.main()
