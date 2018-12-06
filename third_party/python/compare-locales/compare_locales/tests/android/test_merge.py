# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from compare_locales.merge import merge_channels


class TestMerge(unittest.TestCase):
    name = "strings.xml"

    def test_no_changes(self):
        channels = (b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- bar -->
  <string name="foo">value</string>
</resources>
''', b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- bar -->
  <string name="foo">value</string>
</resources>
''')
        self.assertEqual(
            merge_channels(self.name, channels), b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- bar -->
  <string name="foo">value</string>
</resources>
''')

    def test_a_and_b(self):
        channels = (b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- Foo -->
  <string name="foo">value</string>
</resources>
''', b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- Bar -->
  <string name="bar">other value</string>
</resources>
''')
        self.assertEqual(
            merge_channels(self.name, channels), b'''\
<?xml version="1.0" encoding="utf-8"?>
<resources>
  <!-- Bar -->
  <string name="bar">other value</string>
  <!-- Foo -->
  <string name="foo">value</string>
</resources>
''')
