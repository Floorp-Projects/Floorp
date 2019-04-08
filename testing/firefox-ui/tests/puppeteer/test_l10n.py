# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.api.l10n import L10n
from marionette_driver import By
from marionette_driver.errors import NoSuchElementException
from marionette_harness import MarionetteTestCase


class TestL10n(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestL10n, self).setUp()

        self.l10n = L10n(self.marionette)

    def test_dtd_entity_chrome(self):
        dtds = ['chrome://branding/locale/brand.dtd',
                'chrome://browser/locale/baseMenuOverlay.dtd']

        value = self.l10n.localize_entity(dtds, 'helpSafeMode.label')
        elm = self.marionette.find_element(By.ID, 'helpSafeMode')
        self.assertEqual(value, elm.get_attribute('label'))

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_entity, dtds, 'notExistent')

    def test_properties(self):
        properties = ['chrome://global/locale/filepicker.properties',
                      'chrome://global/locale/findbar.properties']

        # TODO: Find a way to verify the retrieved translated string
        value = self.l10n.localize_property(properties, 'NotFound')
        self.assertNotEqual(value, '')

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_property, properties, 'notExistent')
