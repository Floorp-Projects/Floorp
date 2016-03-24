# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozversion
from marionette_driver.errors import MarionetteException

from firefox_ui_harness.testcases import FirefoxTestCase


class TestAppInfo(FirefoxTestCase):

    def test_valid_properties(self):
        binary = self.marionette.bin
        version_info = mozversion.get_version(binary=binary)

        self.assertEqual(self.appinfo.ID, version_info['application_id'])
        self.assertEqual(self.appinfo.name, version_info['application_name'])
        self.assertEqual(self.appinfo.vendor, version_info['application_vendor'])
        self.assertEqual(self.appinfo.version, version_info['application_version'])
        self.assertEqual(self.appinfo.platformBuildID, version_info['platform_buildid'])
        self.assertEqual(self.appinfo.platformVersion, version_info['platform_version'])
        self.assertIsNotNone(self.appinfo.locale)
        self.assertIsNotNone(self.appinfo.user_agent)
        self.assertIsNotNone(self.appinfo.XPCOMABI)

    def test_invalid_properties(self):
        with self.assertRaises(AttributeError):
            self.appinfo.unknown
