# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.ui.update_wizard import UpdateWizardDialog
from marionette_harness import MarionetteTestCase


class TestUpdateWizard(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestUpdateWizard, self).setUp()

        def opener(win):
            self.marionette.execute_script("""
              let updatePrompt = Components.classes["@mozilla.org/updates/update-prompt;1"]
                                 .createInstance(Components.interfaces.nsIUpdatePrompt);
              updatePrompt.checkForUpdates();
            """)

        self.dialog = self.browser.open_window(callback=opener,
                                               expected_window_class=UpdateWizardDialog)
        self.wizard = self.dialog.wizard

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
        finally:
            super(TestUpdateWizard, self).tearDown()

    def test_basic(self):
        self.assertEqual(self.dialog.window_type, 'Update:Wizard')
        self.assertNotEqual(self.dialog.dtds, [])
        self.assertNotEqual(self.dialog.properties, [])

    def test_elements(self):
        """Test correct retrieval of elements."""
        self.assertEqual(self.wizard.element.get_property('localName'), 'wizard')

        buttons = ('cancel_button', 'extra1_button', 'extra2_button',
                   'finish_button', 'next_button', 'previous_button',
                   )
        for button in buttons:
            self.assertEqual(getattr(self.wizard, button).get_property('localName'),
                             'button')

        panels = ('checking', 'downloading', 'dummy', 'error_patching', 'error',
                  'error_extra', 'finished', 'finished_background',
                  'manual_update', 'no_updates_found', 'updates_found_basic',
                  )
        for panel in panels:
            self.assertEqual(getattr(self.wizard, panel).element.get_property('localName'),
                             'wizardpage')

        # elements of the checking panel
        self.assertEqual(self.wizard.checking.progress.get_property('localName'),
                         'progressmeter')

        # elements of the downloading panel
        self.assertEqual(self.wizard.downloading.progress.get_property('localName'),
                         'progressmeter')
