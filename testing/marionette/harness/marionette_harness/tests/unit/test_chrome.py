#Copyright 2007-2009 WebDriver committers
#Copyright 2007-2009 Google Inc.
#
#Licensed under the Apache License, Version 2.0 (the "License");
#you may not use this file except in compliance with the License.
#You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing, software
#distributed under the License is distributed on an "AS IS" BASIS,
#WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#See the License for the specific language governing permissions and
#limitations under the License.

from marionette_driver import By

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class ChromeTests(WindowManagerMixin, MarionetteTestCase):

    def setUp(self):
        super(ChromeTests, self).setUp()

        self.marionette.set_context('chrome')

    def tearDown(self):
        self.close_all_windows()
        super(ChromeTests, self).tearDown()

    def test_hang_until_timeout(self):
        def open_with_menu():
            menu = self.marionette.find_element(By.ID, 'aboutName')
            menu.click()

        new_window = self.open_window(trigger=open_with_menu)
        self.marionette.switch_to_window(new_window)

        try:
            try:
                # Raise an exception type which should not be thrown by Marionette
                # while running this test. Otherwise it would mask eg. IOError as
                # thrown for a socket timeout.
                raise NotImplementedError('Exception should not cause a hang when '
                                          'closing the chrome window')
            finally:
                self.marionette.close_chrome_window()
                self.marionette.switch_to_window(self.start_window)
        except NotImplementedError:
            pass
