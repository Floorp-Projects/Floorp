# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By
from marionette_driver.errors import MarionetteException

from firefox_puppeteer.api.l10n import L10n
from firefox_ui_harness.testcases import FirefoxTestCase


class TestL10n(FirefoxTestCase):

    def setUp(self):
        FirefoxTestCase.setUp(self)
        self.l10n = L10n(lambda: self.marionette)

    def tearDown(self):
        FirefoxTestCase.tearDown(self)

    def test_dtd_entity_chrome(self):
        dtds = ['chrome://global/locale/about.dtd',
                'chrome://browser/locale/baseMenuOverlay.dtd']

        value = self.l10n.get_entity(dtds, 'helpSafeMode.label')
        elm = self.marionette.find_element(By.ID, 'helpSafeMode')
        self.assertEqual(value, elm.get_attribute('label'))

        self.assertRaises(MarionetteException, self.l10n.get_entity, dtds, 'notExistent')

    def test_dtd_entity_content(self):
        dtds = ['chrome://global/locale/about.dtd',
                'chrome://global/locale/aboutSupport.dtd']

        value = self.l10n.get_entity(dtds, 'aboutSupport.pageTitle')

        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)
        self.marionette.navigate('about:support')

        elm = self.marionette.find_element(By.TAG_NAME, 'title')
        self.assertEqual(value, elm.text)

    def test_properties(self):
        properties = ['chrome://global/locale/filepicker.properties',
                      'chrome://global/locale/findbar.properties']

        # TODO: Find a way to verify the retrieved translated string
        value = self.l10n.get_property(properties, 'NotFound')
        self.assertNotEqual(value, '')

        self.assertRaises(MarionetteException, self.l10n.get_property, properties, 'notExistent')
