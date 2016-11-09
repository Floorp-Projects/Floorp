# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By, Wait

from firefox_puppeteer.ui.update_wizard.wizard import Wizard
from firefox_puppeteer.ui.windows import BaseWindow, Windows


# Bug 1143020 - Subclass from BaseDialog ui class with possible wizard mixin
class UpdateWizardDialog(BaseWindow):
    """Representation of the old Software Update Wizard Dialog."""
    window_type = 'Update:Wizard'

    dtds = [
        'chrome://branding/locale/brand.dtd',
        'chrome://mozapps/locale/update/updates.dtd',
    ]

    properties = [
        'chrome://branding/locale/brand.properties',
        'chrome://mozapps/locale/update/updates.properties',
    ]

    def __init__(self, *args, **kwargs):
        BaseWindow.__init__(self, *args, **kwargs)

    @property
    def wizard(self):
        """The :class:`Wizard` instance which represents the wizard.

        :returns: Reference to the wizard.
        """
        # The deck is also the root element
        wizard = self.marionette.find_element(By.ID, 'updates')
        return Wizard(self.marionette, self, wizard)

    def select_next_page(self):
        """Clicks on "Next" button, and waits for the next page to show up."""
        current_panel = self.wizard.selected_panel

        self.wizard.next_button.click()
        Wait(self.marionette).until(
            lambda _: self.wizard.selected_panel != current_panel,
            message='Next panel has not been selected.')


Windows.register_window(UpdateWizardDialog.window_type, UpdateWizardDialog)
