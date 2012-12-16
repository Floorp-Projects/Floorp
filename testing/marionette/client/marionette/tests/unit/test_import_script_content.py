
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from marionette_test import MarionetteTestCase

class TestImportScriptContent(MarionetteTestCase):

    def test_importing_script_then_reusing_it(self):
        test_html = self.marionette.absolute_url("test_windows.html")
        self.marionette.navigate(test_html)
        js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importscript.js"))
        self.current_window = self.marionette.current_window_handle
        link = self.marionette.find_element("link text", "Open new window")
        link.click()

        windows = self.marionette.window_handles
        windows.remove(self.current_window)
        self.marionette.switch_to_window(windows[0])

        self.marionette.import_script(js)
        self.marionette.close()

        self.marionette.switch_to_window(self.current_window)
        self.assertEqual("i'm a test function!", self.marionette.execute_script("return testFunc();"))

