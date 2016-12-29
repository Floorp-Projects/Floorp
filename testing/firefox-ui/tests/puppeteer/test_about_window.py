# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from firefox_puppeteer.ui.deck import Panel
from marionette_harness import MarionetteTestCase


class TestAboutWindow(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestAboutWindow, self).setUp()

        self.about_window = self.browser.open_about_window()
        self.deck = self.about_window.deck

    def tearDown(self):
        try:
            self.puppeteer.windows.close_all([self.browser])
        finally:
            super(TestAboutWindow, self).tearDown()

    def test_basic(self):
        self.assertEqual(self.about_window.window_type, 'Browser:About')

    def test_elements(self):
        """Test correct retrieval of elements."""
        self.assertNotEqual(self.about_window.dtds, [])

        self.assertEqual(self.deck.element.get_property('localName'), 'deck')

        # apply panel
        panel = self.deck.apply
        self.assertEqual(panel.element.get_property('localName'), 'hbox')
        self.assertEqual(panel.button.get_property('localName'), 'button')

        # check_for_updates panel
        panel = self.deck.check_for_updates
        self.assertEqual(panel.element.get_property('localName'), 'hbox')
        self.assertEqual(panel.button.get_property('localName'), 'button')

        # checking_for_updates panel
        self.assertEqual(self.deck.checking_for_updates.element.get_property('localName'), 'hbox')

        # download_and_install panel
        panel = self.deck.download_and_install
        self.assertEqual(panel.element.get_property('localName'), 'hbox')
        self.assertEqual(panel.button.get_property('localName'), 'button')

        # download_failed panel
        self.assertEqual(self.deck.download_failed.element.get_property('localName'), 'hbox')

        # downloading panel
        self.assertEqual(self.deck.downloading.element.get_property('localName'), 'hbox')

        # check deck attributes
        self.assertIsInstance(self.deck.selected_index, int)
        self.assertIsInstance(self.deck.selected_panel, Panel)

    def test_open_window(self):
        """Test various opening strategies."""
        def opener(win):
            self.browser.menubar.select_by_id('helpMenu', 'aboutName')

        open_strategies = ('menu',
                           opener,
                           )

        self.about_window.close()
        for trigger in open_strategies:
            about_window = self.browser.open_about_window(trigger=trigger)
            self.assertEquals(about_window, self.puppeteer.windows.current)
            about_window.close()
