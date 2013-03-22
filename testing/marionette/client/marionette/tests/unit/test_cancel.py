# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette_test import MarionetteTestCase
from marionette import Actions

class testSingleFinger(MarionetteTestCase):
    def test_chain(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCancel")
        action = Actions(self.marionette)
        action.press(button).wait(5).cancel()
        action.perform()
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCancel').innerHTML;"))

    def test_element(self):
        testTouch = self.marionette.absolute_url("testAction.html")
        self.marionette.navigate(testTouch)
        button = self.marionette.find_element("id", "mozLinkCancel")
        new_id = button.press()
        button.cancel_touch(new_id)
        time.sleep(15)
        self.assertEqual("End", self.marionette.execute_script("return document.getElementById('mozLinkCancel').innerHTML;"))
