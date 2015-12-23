# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By

from firefox_puppeteer.ui.about_window.deck import Deck
from firefox_puppeteer.ui.windows import BaseWindow, Windows


class AboutWindow(BaseWindow):
    """Representation of the About window."""
    window_type = 'Browser:About'

    dtds = [
        'chrome://branding/locale/brand.dtd',
        'chrome://browser/locale/aboutDialog.dtd',
    ]

    def __init__(self, *args, **kwargs):
        BaseWindow.__init__(self, *args, **kwargs)

    @property
    def deck(self):
        """The :class:`Deck` instance which represents the deck.

        :returns: Reference to the deck.
        """
        self.switch_to()

        deck = self.window_element.find_element(By.ID, 'updateDeck')
        return Deck(lambda: self.marionette, self, deck)


Windows.register_window(AboutWindow.window_type, AboutWindow)
