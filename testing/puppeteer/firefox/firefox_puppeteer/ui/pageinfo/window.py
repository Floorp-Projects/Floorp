# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By

from firefox_puppeteer.ui.pageinfo.deck import Deck
from firefox_puppeteer.ui.windows import BaseWindow, Windows


class PageInfoWindow(BaseWindow):
    """Representation of a page info window."""

    window_type = 'Browser:page-info'

    dtds = [
        'chrome://browser/locale/pageInfo.dtd',
    ]

    properties = [
        'chrome://browser/locale/browser.properties',
        'chrome://browser/locale/pageInfo.properties',
        'chrome://pippki/locale/pippki.properties',
    ]

    def __init__(self, *args, **kwargs):
        BaseWindow.__init__(self, *args, **kwargs)

    @property
    def deck(self):
        """The :class:`Deck` instance which represents the deck.

        :returns: Reference to the deck.
        """
        deck = self.window_element.find_element(By.ID, 'mainDeck')
        return Deck(lambda: self.marionette, self, deck)

    def close(self, trigger='shortcut', force=False):
        """Closes the current page info window by using the specified trigger.

        :param trigger: Optional, method to close the current window. This can
         be a string with one of `menu` (OS X only) or `shortcut`, or a callback
         which gets triggered with the current :class:`PageInfoWindow` as parameter.
         Defaults to `shortcut`.

        :param force: Optional, forces the closing of the window by using the Gecko API.
         Defaults to `False`.
        """
        def callback(win):
            # Prepare action which triggers the opening of the browser window
            if callable(trigger):
                trigger(win)
            elif trigger == 'menu':
                self.menubar.select_by_id('file-menu', 'menu_close')
            elif trigger == 'shortcut':
                win.send_shortcut(win.get_entity('closeWindow.key'),
                                  accel=True)
            else:
                raise ValueError('Unknown closing method: "%s"' % trigger)

        BaseWindow.close(self, callback, force)

Windows.register_window(PageInfoWindow.window_type, PageInfoWindow)
