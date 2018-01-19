# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver import By
from marionette_driver.errors import (
    InvalidArgumentException,
    NoSuchElementException,
    UnknownException
)
from marionette_driver.localization import L10n

from marionette_harness import MarionetteTestCase


class TestL10n(MarionetteTestCase):

    def setUp(self):
        super(TestL10n, self).setUp()

        self.l10n = L10n(self.marionette)

    def test_localize_entity(self):
        dtds = ['chrome://marionette/content/test_dialog.dtd']
        value = self.l10n.localize_entity(dtds, 'testDialog.title')

        self.assertEqual(value, 'Test Dialog')

    def test_localize_entity_invalid_arguments(self):
        dtds = ['chrome://marionette/content/test_dialog.dtd']

        self.assertRaises(NoSuchElementException,
                          self.l10n.localize_entity, dtds, 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_entity, dtds[0], 'notExistent')
        self.assertRaises(InvalidArgumentException,
                          self.l10n.localize_entity, dtds, True)

    def test_localize_property(self):
        properties = ['chrome://marionette/content/test_dialog.properties']

        value = self.l10n.localize_property(properties, 'testDialog.title')
        self.assertEqual(value, 'Test Dialog')

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
