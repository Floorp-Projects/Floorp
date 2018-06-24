# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import urllib

from marionette_driver import By, errors, Wait
from marionette_driver.keys import Keys
from marionette_driver.marionette import WEB_ELEMENT_KEY

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(urllib.quote(doc))


class Actions(object):
    """Temporary class until Marionette client supports the WebDriver actions."""

    def __init__(self, marionette):
        self.marionette = marionette

        self.action_chain = []

    def perform(self):
        params = {"actions": [{
            "actions": self.action_chain,
            "id": "mouse",
            "parameters": {
                "pointerType": "mouse"
            },
            "type": "pointer"
        }]}

        return self.marionette._send_message("performActions", params=params)

    def move(self, element, x=0, y=0, duration=250):
        self.action_chain.append({
            "duration": duration,
            "origin": {WEB_ELEMENT_KEY: element.id},
            "type": "pointerMove",
            "x": x,
            "y": y,
        })

        return self

    def click(self):
        self.action_chain.extend([{
            "button": 0,
            "type": "pointerDown"
        }, {
            "button": 0,
            "type": "pointerUp"
        }])

        return self


class BaseMouseAction(MarionetteTestCase):

    def setUp(self):
        super(BaseMouseAction, self).setUp()

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

        self.action = Actions(self.marionette)

    def tearDown(self):
        super(BaseMouseAction, self).tearDown()

    @property
    def click_position(self):
        return self.marionette.execute_script("""
          if (window.click_x && window.click_y) {
            return {x: window.click_x, y: window.click_y};
          }
        """, sandbox=None)

    def get_element_center_point(self, elem):
        return {
            "x": elem.rect["x"] + elem.rect["width"] / 2,
            "y": elem.rect["y"] + elem.rect["height"] / 2
        }


class TestNonSpecCompliantPointerOrigin(BaseMouseAction):

    def setUp(self):
        super(TestNonSpecCompliantPointerOrigin, self).setUp()

        self.marionette.delete_session()
        self.marionette.start_session({"moz:useNonSpecCompliantPointerOrigin": True})

    def tearDown(self):
        self.marionette.delete_session()
        self.marionette.start_session()

        super(TestNonSpecCompliantPointerOrigin, self).tearDown()

    def test_click_element_smaller_than_viewport(self):
        self.marionette.navigate(inline("""
          <div id="div" style="width: 10vw; height: 10vh; background: green;"
               onclick="window.click_x = event.clientX; window.click_y = event.clientY" />
        """))
        elem = self.marionette.find_element(By.ID, "div")
        elem_center_point = self.get_element_center_point(elem)

        self.action.move(elem).click().perform()
        click_position = Wait(self.marionette).until(lambda _: self.click_position,
                                                     message="No click event has been detected")
        self.assertAlmostEqual(click_position["x"], elem_center_point["x"], delta=1)
        self.assertAlmostEqual(click_position["y"], elem_center_point["y"], delta=1)

    def test_click_element_larger_than_viewport_with_center_point_inside(self):
        self.marionette.navigate(inline("""
          <div id="div" style="width: 150vw; height: 150vh; background: green;"
               onclick="window.click_x = event.clientX; window.click_y = event.clientY" />
        """))
        elem = self.marionette.find_element(By.ID, "div")
        elem_center_point = self.get_element_center_point(elem)

        self.action.move(elem).click().perform()
        click_position = Wait(self.marionette).until(lambda _: self.click_position,
                                                     message="No click event has been detected")
        self.assertAlmostEqual(click_position["x"], elem_center_point["x"], delta=1)
        self.assertAlmostEqual(click_position["y"], elem_center_point["y"], delta=1)

    def test_click_element_larger_than_viewport_with_center_point_outside(self):
        self.marionette.navigate(inline("""
          <div id="div" style="width: 300vw; height: 300vh; background: green;"
               onclick="window.click_x = event.clientX; window.click_y = event.clientY" />
        """))
        elem = self.marionette.find_element(By.ID, "div")

        with self.assertRaises(errors.MoveTargetOutOfBoundsException):
            self.action.move(elem).click().perform()
