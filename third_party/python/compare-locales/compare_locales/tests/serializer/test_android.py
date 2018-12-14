# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from . import Helper


class TestAndroidSerializer(Helper, unittest.TestCase):
    name = 'strings.xml'
    reference_content = """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <!-- The page html title (i.e. the <title> tag content) -->
    <string name="title">Unable to connect</string>
    <string name="message"><![CDATA[
      <ul>
        <li>The site could be temporarily unavailable or too busy.</li>
      </ul>
    ]]></string>
    <string name="wrapped_message">
      <![CDATA[
        <ul>
          <li>The site could be temporarily unavailable or too busy.</li>
        </ul>
      ]]>
    </string>
</resources>
"""

    def test_nothing_new_or_old(self):
        self._test(
            "",
            {},
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    </resources>
"""
        )

    def test_new_string(self):
        self._test(
            "",
            {
                "title": "Cannot connect"
            },
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <!-- The page html title (i.e. the <title> tag content) -->
    <string name="title">Cannot connect</string>
    </resources>
"""
        )

    def test_new_cdata(self):
        self._test(
            "",
            {
                "message": """
<ul>
  <li>Something else</li>
</ul>
"""
            },
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="message"><![CDATA[
<ul>
  <li>Something else</li>
</ul>
]]></string>
    </resources>
"""
        )

    def test_new_cdata_wrapped(self):
        self._test(
            "",
            {
                "wrapped_message": """
<ul>
  <li>Something else</li>
</ul>
"""
            },
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="wrapped_message">
      <![CDATA[
<ul>
  <li>Something else</li>
</ul>
]]>
    </string>
</resources>
"""
        )

    def test_remove_string(self):
        self._test(
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="first_old_title">Unable to connect</string>
    <string name="title">Unable to connect</string>
    <string name="last_old_title">Unable to connect</string>
</resources>
""",
            {},
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="title">Unable to connect</string>
    </resources>
"""
        )

    def test_same_string(self):
        self._test(
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="title">Unable to connect</string>
</resources>
""",
            {
                "title": "Unable to connect"
            },
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <!-- The page html title (i.e. the <title> tag content) -->
    <string name="title">Unable to connect</string>
    </resources>
"""
        )


class TestAndroidDuplicateComment(Helper, unittest.TestCase):
    name = 'strings.xml'
    reference_content = """\
<?xml version="1.0" encoding="utf-8"?>
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->
<resources>
    <!-- Label used in the contextmenu shown when long-pressing on a link -->
    <string name="contextmenu_open_in_app">Open with app</string>
    <!-- Label used in the contextmenu shown when long-pressing on a link -->
    <string name="contextmenu_link_share">Share link</string>
</resources>
"""

    def test_missing_translation(self):
        self._test(
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>

    <!-- Label used in the contextmenu shown when long-pressing on a link -->
    <!-- Label used in the contextmenu shown when long-pressing on a link -->
    <string name="contextmenu_link_share"/>
  </resources>
""",
            {
                "contextmenu_link_share": "translation"
            },
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>

    <!-- Label used in the contextmenu shown when long-pressing on a link -->
    <string name="contextmenu_link_share">translation</string>
  </resources>
"""
        )


class TestAndroidTools(Helper, unittest.TestCase):
    name = 'strings.xml'
    reference_content = (
        """\
<resources xmlns:tools="http://schemas.android.com/tools">
    <string name="app_tagline">Take your passwords everywhere.</string>
    <string name="search_your_entries" tools:ignore="ExtraTranslation">"""
        "search your entries"
        """</string>
</resources>
""")

    def test_namespaced_document(self):
        self._test(
            """\
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_tagline">Localized tag line</string>
  </resources>
""",
            {
                "search_your_entries": "Looking for Entries"
            },
            (
                """\
<?xml version="1.0" encoding="utf-8"?>
<resources xmlns:tools="http://schemas.android.com/tools">
    <string name="app_tagline">Localized tag line</string>
    <string name="search_your_entries" tools:ignore="ExtraTranslation">"""
                "Looking for Entries"
                """</string>
</resources>
""")
        )
