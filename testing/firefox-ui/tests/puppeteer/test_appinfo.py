# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozversion

from firefox_puppeteer import PuppeteerMixin
from marionette_harness import MarionetteTestCase


class TestAppInfo(PuppeteerMixin, MarionetteTestCase):

    def test_valid_properties(self):
        binary = self.marionette.bin
        version_info = mozversion.get_version(binary=binary)

        self.assertEqual(self.puppeteer.appinfo.ID, version_info['application_id'])
        self.assertEqual(self.puppeteer.appinfo.name, version_info['application_name'])
        self.assertEqual(self.puppeteer.appinfo.vendor, version_info['application_vendor'])
        self.assertEqual(self.puppeteer.appinfo.version, version_info['application_version'])
        # Bug 1298328 - Platform buildid mismatch due to incremental builds
        # self.assertEqual(self.puppeteer.appinfo.platformBuildID,
        #                  version_info['platform_buildid'])
        self.assertEqual(self.puppeteer.appinfo.platformVersion, version_info['platform_version'])
        self.assertIsNotNone(self.puppeteer.appinfo.locale)
        self.assertIsNotNone(self.puppeteer.appinfo.user_agent)
        self.assertIsNotNone(self.puppeteer.appinfo.XPCOMABI)

    def test_invalid_properties(self):
        with self.assertRaises(AttributeError):
            self.puppeteer.appinfo.unknown
