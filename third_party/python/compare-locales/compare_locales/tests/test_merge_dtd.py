# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMergeDTD(unittest.TestCase):
    name = "foo.dtd"
    maxDiff = None

    def test_no_changes(self):
        channels = (b"""
<!ENTITY foo "Foo 1">
""", b"""
<!ENTITY foo "Foo 2">
""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
<!ENTITY foo "Foo 1">
""")

    def test_trailing_whitespace(self):
        channels = (b"""
<!ENTITY foo "Foo 1">
""", b"""
<!ENTITY foo "Foo 2"> \n""")
        self.assertEqual(
            merge_channels(self.name, *channels), b"""
<!ENTITY foo "Foo 1"> \n""")

    def test_browser_dtd(self):
        channels = (b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE : FILE This file contains the browser main menu ... -->
<!-- LOCALIZATION NOTE : FILE Do not translate commandkeys -->

<!-- LOCALIZATION NOTE (mainWindow.titlemodifier) : DONT_TRANSLATE -->
<!ENTITY mainWindow.titlemodifier "&brandFullName;">
<!-- LOCALIZATION NOTE (mainWindow.separator): DONT_TRANSLATE -->
<!ENTITY mainWindow.separator " - ">
<!-- LOCALIZATION NOTE (mainWindow.privatebrowsing2): This will be appended ...
                                                      inside the ... -->
<!ENTITY mainWindow.privatebrowsing2 "(Private Browsing)">
""", b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE : FILE This file contains the browser main menu ... -->
<!-- LOCALIZATION NOTE : FILE Do not translate commandkeys -->

<!-- LOCALIZATION NOTE (mainWindow.title): DONT_TRANSLATE -->
<!ENTITY mainWindow.title "&brandFullName;">
<!-- LOCALIZATION NOTE (mainWindow.titlemodifier) : DONT_TRANSLATE -->
<!ENTITY mainWindow.titlemodifier "&brandFullName;">
<!-- LOCALIZATION NOTE (mainWindow.privatebrowsing): This will be appended ...
                                                     inside the ... -->
<!ENTITY mainWindow.privatebrowsing "(Private Browsing)">
""")

        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE : FILE This file contains the browser main menu ... -->
<!-- LOCALIZATION NOTE : FILE Do not translate commandkeys -->

<!-- LOCALIZATION NOTE (mainWindow.title): DONT_TRANSLATE -->
<!ENTITY mainWindow.title "&brandFullName;">

<!-- LOCALIZATION NOTE (mainWindow.titlemodifier) : DONT_TRANSLATE -->
<!ENTITY mainWindow.titlemodifier "&brandFullName;">
<!-- LOCALIZATION NOTE (mainWindow.privatebrowsing): This will be appended ...
                                                     inside the ... -->
<!ENTITY mainWindow.privatebrowsing "(Private Browsing)">
<!-- LOCALIZATION NOTE (mainWindow.separator): DONT_TRANSLATE -->
<!ENTITY mainWindow.separator " - ">
<!-- LOCALIZATION NOTE (mainWindow.privatebrowsing2): This will be appended ...
                                                      inside the ... -->
<!ENTITY mainWindow.privatebrowsing2 "(Private Browsing)">
""")

    def test_aboutServiceWorkers_dtd(self):
        channels = (b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY title                     "About Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY maintitle                 "Registered Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_not_enabled       "Service Workers are not enabled.">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_no_serviceworkers "No Service Workers registered.">
""", b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY title                     "About Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY maintitle                 "Registered Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_not_enabled       "Service Workers are not enabled.">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_no_serviceworkers "No Service Workers registered.">
""")

        self.assertMultiLineEqual(
            merge_channels(self.name, *channels), b"""\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY title                     "About Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY maintitle                 "Registered Service Workers">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_not_enabled       "Service Workers are not enabled.">
<!-- LOCALIZATION NOTE the term "Service Workers" should not be translated. -->
<!ENTITY warning_no_serviceworkers "No Service Workers registered.">
""")
