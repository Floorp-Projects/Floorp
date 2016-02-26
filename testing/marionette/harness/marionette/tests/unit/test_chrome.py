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
from marionette_driver.errors import NoSuchElementException
from marionette import MarionetteTestCase


class ChromeTests(MarionetteTestCase):

    def test_hang_until_timeout(self):
        with self.marionette.using_context('chrome'):
            start_handle = self.marionette.current_chrome_window_handle
            menu = self.marionette.find_element(By.ID, 'aboutName')
            menu.click()
            handles = self.marionette.chrome_window_handles
            handles.remove(start_handle)
            self.marionette.switch_to_window(handles[0])
            self.assertRaises(NoSuchElementException, self.marionette.find_element, By.ID, 'dek')

            # Clean up the window
            self.marionette.close()
            self.marionette.switch_to_window(start_handle)
