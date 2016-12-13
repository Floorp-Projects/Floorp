# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from firefox_puppeteer import PuppeteerMixin
from marionette_driver.errors import NoSuchElementException
from marionette_harness import MarionetteTestCase


class TestMenuBar(PuppeteerMixin, MarionetteTestCase):

    def setUp(self):
        super(TestMenuBar, self).setUp()

    def test_click_item_in_menubar(self):
        def opener(_):
            self.browser.menubar.select_by_id('file-menu',
                                              'menu_newNavigatorTab')

        self.browser.tabbar.open_tab(trigger=opener)

        self.browser.tabbar.tabs[-1].close()

    def test_click_non_existent_menu_and_item(self):
        with self.assertRaises(NoSuchElementException):
            self.browser.menubar.select_by_id('foobar-menu',
                                              'menu_newNavigatorTab')

        with self.assertRaises(NoSuchElementException):
            self.browser.menubar.select_by_id('file-menu', 'menu_foobar')
