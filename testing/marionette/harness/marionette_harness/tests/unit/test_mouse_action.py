# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from six.moves.urllib.parse import quote

from marionette_driver import By, errors, Wait
from marionette_driver.keys import Keys

from marionette_harness import MarionetteTestCase


def inline(doc):
    return "data:text/html;charset=utf-8,{}".format(quote(doc))


class BaseMouseAction(MarionetteTestCase):
    def setUp(self):
        super(BaseMouseAction, self).setUp()
        self.mouse_chain = self.marionette.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        )

        if self.marionette.session_capabilities["platformName"] == "mac":
            self.mod_key = Keys.META
        else:
            self.mod_key = Keys.CONTROL

    def tearDown(self):
        self.marionette.actions.release()

        super(BaseMouseAction, self).tearDown()

    @property
    def click_position(self):
        return self.marionette.execute_script(
            """
          if (window.click_x && window.click_y) {
            return {x: window.click_x, y: window.click_y};
          }
        """,
            sandbox=None,
        )

    def get_element_center_point(self, elem):
        # pylint --py3k W1619
        return {
            "x": elem.rect["x"] + elem.rect["width"] / 2,
            "y": elem.rect["y"] + elem.rect["height"] / 2,
        }


class TestPointerActions(BaseMouseAction):
    def test_click_action(self):
        test_html = self.marionette.absolute_url("test.html")
        self.marionette.navigate(test_html)
        link = self.marionette.find_element(By.ID, "mozLink")
        self.mouse_chain.click(element=link).perform()
        self.assertEqual(
            "Clicked",
            self.marionette.execute_script(
                "return document.getElementById('mozLink').innerHTML"
            ),
        )

    def test_clicking_element_out_of_view(self):
        self.marionette.navigate(
            inline(
                """
            <div style="position:relative;top:200vh;">foo</div>
        """
            )
        )
        el = self.marionette.find_element(By.TAG_NAME, "div")
        with self.assertRaises(errors.MoveTargetOutOfBoundsException):
            self.mouse_chain.click(element=el).perform()

    def test_double_click_action(self):
        self.marionette.navigate(
            inline(
                """
          <script>window.eventCount = 0;</script>
          <button onclick="window.eventCount++">foobar</button>
        """
            )
        )

        el = self.marionette.find_element(By.CSS_SELECTOR, "button")
        self.mouse_chain.click(el).pause(100).click(el).perform()

        event_count = self.marionette.execute_script(
            "return window.eventCount", sandbox=None
        )
        self.assertEqual(event_count, 2)

    def test_context_click_action(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)
        click_el = self.marionette.find_element(By.ID, "normal")

        def context_menu_state():
            with self.marionette.using_context("chrome"):
                cm_el = self.marionette.find_element(By.ID, "contentAreaContextMenu")
                return cm_el.get_property("state")

        self.assertEqual("closed", context_menu_state())
        self.mouse_chain.click(element=click_el, button=2).perform()
        Wait(self.marionette).until(
            lambda _: context_menu_state() == "open",
            message="Context menu did not open",
        )
        with self.marionette.using_context("chrome"):
            cm_el = self.marionette.find_element(By.ID, "contentAreaContextMenu")
            self.marionette.execute_script(
                "arguments[0].hidePopup()", script_args=(cm_el,)
            )
        Wait(self.marionette).until(
            lambda _: context_menu_state() == "closed",
            message="Context menu did not close",
        )

    def test_middle_click_action(self):
        test_html = self.marionette.absolute_url("clicks.html")
        self.marionette.navigate(test_html)

        self.marionette.find_element(By.ID, "addbuttonlistener").click()

        el = self.marionette.find_element(By.ID, "showbutton")
        self.mouse_chain.click(element=el, button=1).perform()

        Wait(self.marionette).until(
            lambda _: el.get_property("innerHTML") == "1",
            message="Middle-click hasn't been fired",
        )
