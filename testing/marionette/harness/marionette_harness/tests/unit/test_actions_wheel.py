# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver import By
from marionette_harness import MarionetteTestCase, parameterized


class BaseWheelAction(MarionetteTestCase):
    def setUp(self):
        super(BaseWheelAction, self).setUp()

        self.test_page = self.marionette.absolute_url("actions_scroll.html")
        self.marionette.navigate(self.test_page)

        self.wheel_chain = self.marionette.actions.sequence("wheel", "wheel_id")

    def tearDown(self):
        self.marionette.actions.release()

        super(BaseWheelAction, self).tearDown()

    def get_events(self):
        return self.marionette.execute_script("return allEvents.events;", sandbox=None)


class TestWheelAction(BaseWheelAction):
    def test_scroll_not_scrollable(self):
        target = self.marionette.find_element(By.ID, "not-scrollable")

        self.wheel_chain.scroll(0, 0, 5, 10, origin=target, duration=0).perform()

        events = self.get_events()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["type"], "wheel")
        self.assertEqual(events[0]["deltaX"], 5)
        self.assertEqual(events[0]["deltaY"], 10)
        self.assertEqual(events[0]["deltaZ"], 0)
        self.assertEqual(events[0]["target"], "not-scrollable-content")

    def test_scroll_scrollable(self):
        target = self.marionette.find_element(By.ID, "scrollable")
        self.wheel_chain.scroll(0, 0, 5, 10, origin=target).perform()

        events = self.get_events()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["type"], "wheel")
        self.assertEqual(events[0]["deltaX"], 5)
        self.assertEqual(events[0]["deltaY"], 10)
        self.assertEqual(events[0]["deltaZ"], 0)
        self.assertEqual(events[0]["target"], "scrollable-content")

    def test_scroll_iframe_scrollable(self):
        iframe = self.marionette.find_element(By.ID, "iframe")
        self.marionette.switch_to_frame(iframe)

        target = self.marionette.find_element(By.ID, "iframeContent")
        self.wheel_chain.scroll(0, 0, 5, 10, origin=target).perform()

        self.marionette.switch_to_frame()

        events = self.get_events()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["type"], "wheel")
        self.assertEqual(events[0]["deltaX"], 5)
        self.assertEqual(events[0]["deltaY"], 10)
        self.assertEqual(events[0]["deltaZ"], 0)
        self.assertEqual(events[0]["target"], "iframeContent")
