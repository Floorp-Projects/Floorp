# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MarionetteTestCase
from marionette_driver import By
from marionette_driver.errors import InvalidArgumentException, NoSuchElementException
from marionette_driver.localization import L10n


class TestL10n(MarionetteTestCase):

    def setUp(self):
        super(TestL10n, self).setUp()

        self.l10n = L10n(self.marionette)

    def test_localize_entity_chrome(self):
        dtds = ['chrome://global/locale/about.dtd',
                'chrome://browser/locale/baseMenuOverlay.dtd']

        with self.marionette.using_context('chrome'):
            value = self.l10n.localize_entity(dtds, 'helpSafeMode.label')
            element = self.marionette.find_element(By.ID, 'helpSafeMode')
            self.assertEqual(value, element.get_attribute('label'))

    def test_localize_entity_content(self):
        dtds = ['chrome://global/locale/about.dtd',
                'chrome://global/locale/aboutSupport.dtd']

        value = self.l10n.localize_entity(dtds, 'aboutSupport.pageTitle')
        self.marionette.navigate('about:support')
        element = self.marionette.find_element(By.TAG_NAME, 'title')
        self.assertEqual(value, element.text)

    def test_localize_entity_invalid_arguments(self):
        dtds = ['chrome://global/locale/about.dtd']

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_entity, dtds, 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_entity, dtds[0], 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_entity, dtds, True)

    def test_localize_property(self):
        properties = ['chrome://global/locale/filepicker.properties',
                      'chrome://global/locale/findbar.properties']

        # TODO: Find a way to verify the retrieved localized value
        value = self.l10n.localize_property(properties, 'CaseSensitive')
        self.assertNotEqual(value, '')

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_property, properties, 'notExistent')

    def test_localize_property_invalid_arguments(self):
        properties = ['chrome://global/locale/filepicker.properties']

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_property, properties, 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_property, properties[0], 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_property, properties, True)
