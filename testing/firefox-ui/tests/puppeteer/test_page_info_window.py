# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By
from firefox_ui_harness.testcases import FirefoxTestCase


class TestPageInfoWindow(FirefoxTestCase):

    def tearDown(self):
        try:
            self.windows.close_all([self.browser])
        finally:
            FirefoxTestCase.tearDown(self)

    def test_elements(self):
        """Test correct retrieval of elements."""
        page_info = self.browser.open_page_info_window()

        self.assertNotEqual(page_info.dtds, [])
        self.assertNotEqual(page_info.properties, [])

        self.assertEqual(page_info.deck.element.get_attribute('localName'), 'deck')

        # feed panel
        self.assertEqual(page_info.deck.feed.element.get_attribute('localName'), 'vbox')

        # general panel
        self.assertEqual(page_info.deck.general.element.get_attribute('localName'), 'vbox')

        # media panel
        self.assertEqual(page_info.deck.media.element.get_attribute('localName'), 'vbox')

        # permissions panel
        self.assertEqual(page_info.deck.permissions.element.get_attribute('localName'), 'vbox')

        # security panel
        panel = page_info.deck.select(page_info.deck.security)

        self.assertEqual(panel.element.get_attribute('localName'), 'vbox')

        self.assertEqual(panel.domain.get_attribute('localName'), 'textbox')
        self.assertEqual(panel.owner.get_attribute('localName'), 'textbox')
        self.assertEqual(panel.verifier.get_attribute('localName'), 'textbox')

        self.assertEqual(panel.view_certificate.get_attribute('localName'), 'button')
        self.assertEqual(panel.view_cookies.get_attribute('localName'), 'button')
        self.assertEqual(panel.view_passwords.get_attribute('localName'), 'button')

    def test_select(self):
        """Test properties and methods for switching between panels."""
        page_info = self.browser.open_page_info_window()
        deck = page_info.deck

        self.assertEqual(deck.selected_panel, deck.general)

        self.assertEqual(deck.select(deck.security), deck.security)
        self.assertEqual(deck.selected_panel, deck.security)

    def test_open_window(self):
        """Test various opening strategies."""
        def opener(win):
            self.browser.menubar.select_by_id('tools-menu', 'menu_pageInfo')

        open_strategies = ('menu',
                           'shortcut',
                           opener,
                           )

        for trigger in open_strategies:
            if trigger == 'shortcut' and self.platform == 'windows_nt':
                # The shortcut for page info window does not exist on windows.
                self.assertRaises(ValueError, self.browser.open_page_info_window,
                                  trigger=trigger)
                continue

            page_info = self.browser.open_page_info_window(trigger=trigger)
            self.assertEquals(page_info, self.windows.current)
            page_info.close()

    def test_close_window(self):
        """Test various closing strategies."""
        def closer(win):
            win.send_shortcut(win.get_entity('closeWindow.key'), accel=True)

        # Close a tab by each trigger method
        close_strategies = ('menu',
                            'shortcut',
                            closer,
                            )
        for trigger in close_strategies:
            # menu only works on OS X
            if trigger == 'menu' and self.platform != 'darwin':
                continue

            page_info = self.browser.open_page_info_window()
            page_info.close(trigger=trigger)
            self.assertTrue(page_info.closed)
