# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.expected import element_present
from marionette_driver import errors
from marionette_driver.marionette import Alert
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, WindowManagerMixin


class BaseAlertTestCase(WindowManagerMixin, MarionetteTestCase):

    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except errors.NoAlertPresentException:
            return False

    def wait_for_alert(self, timeout=None):
        Wait(self.marionette, timeout=timeout).until(
            lambda _: self.alert_present())

    def wait_for_alert_closed(self, timeout=None):
        Wait(self.marionette, timeout=timeout).until(
            lambda _: not self.alert_present())


class TestTabModalAlerts(BaseAlertTestCase):

    def setUp(self):
        super(TestTabModalAlerts, self).setUp()
        self.assertTrue(self.marionette.get_pref("prompts.tab_modal.enabled",
                        "Tab modal alerts should be enabled by default."))

        self.marionette.navigate(self.marionette.absolute_url("test_tab_modal_dialogs.html"))

    def tearDown(self):
        self.marionette.execute_script("window.onbeforeunload = null;")

        # Ensure to close a possible remaining tab modal dialog
        try:
            alert = self.marionette.switch_to_alert()
            alert.dismiss()

            self.wait_for_alert_closed()
        except:
            pass

        super(TestTabModalAlerts, self).tearDown()

    def test_no_alert_raises(self):
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).accept()
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).dismiss()

    def test_alert_accept(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()

    def test_alert_dismiss(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    def test_confirm_accept(self):
        self.marionette.find_element(By.ID, "tab-modal-confirm").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "confirm-result").text == "true")

    def test_confirm_dismiss(self):
        self.marionette.find_element(By.ID, "tab-modal-confirm").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "confirm-result").text == "false")

    def test_prompt_accept(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "prompt-result").text == "")

    def test_prompt_dismiss(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "prompt-result").text == "null")

    def test_alert_opened_before_session_starts(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()

        # Restart the session to ensure we still find the formerly left-open dialog.
        self.marionette.delete_session()
        self.marionette.start_session()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    def test_alert_text(self):
        with self.assertRaises(errors.NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, "Marionette alert")
        alert.accept()

    def test_prompt_text(self):
        with self.assertRaises(errors.NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, "Marionette prompt")
        alert.accept()

    def test_confirm_text(self):
        with self.assertRaises(errors.NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, "tab-modal-confirm").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, "Marionette confirm")
        alert.accept()

    def test_set_text_throws(self):
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).send_keys("Foo")
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        with self.assertRaises(errors.ElementNotInteractableException):
            alert.send_keys("Foo")
        alert.accept()

    def test_set_text_accept(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Some text!")
        alert.accept()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "prompt-result").text == "Some text!")

    def test_set_text_dismiss(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Some text!")
        alert.dismiss()
        self.wait_for_condition(
            lambda mn: mn.find_element(By.ID, "prompt-result").text == "null")

    def test_onbeforeunload_dismiss(self):
        start_url = self.marionette.get_url()
        self.marionette.find_element(By.ID, "onbeforeunload-handler").click()
        self.wait_for_condition(
            lambda mn: mn.execute_script("""
              return window.onbeforeunload !== null;
            """))
        self.marionette.navigate("about:blank")
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertTrue(alert.text.startswith("This page is asking you to confirm"))
        alert.dismiss()
        self.assertTrue(self.marionette.get_url().startswith(start_url))

    def test_onbeforeunload_accept(self):
        self.marionette.find_element(By.ID, "onbeforeunload-handler").click()
        self.wait_for_condition(
            lambda mn: mn.execute_script("""
              return window.onbeforeunload !== null;
            """))
        self.marionette.navigate("about:blank")
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertTrue(alert.text.startswith("This page is asking you to confirm"))
        alert.accept()
        self.wait_for_condition(lambda mn: mn.get_url() == "about:blank")

    def test_unrelated_command_when_alert_present(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        with self.assertRaises(errors.UnexpectedAlertOpen):
            self.marionette.find_element(By.ID, "click-result")

    def test_modal_is_dismissed_after_unexpected_alert(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        with self.assertRaises(errors.UnexpectedAlertOpen):
            self.marionette.find_element(By.ID, "click-result")

        assert not self.alert_present()


class TestModalAlerts(BaseAlertTestCase):

    def setUp(self):
        super(TestModalAlerts, self).setUp()
        self.marionette.set_pref("network.auth.non-web-content-triggered-resources-http-auth-allow",
                                 True)

    def tearDown(self):
        # Ensure to close a possible remaining modal dialog
        self.close_all_windows()
        self.marionette.clear_pref("network.auth.non-web-content-triggered-resources-http-auth-allow")

        super(TestModalAlerts, self).tearDown()

    def test_http_auth_dismiss(self):
        self.marionette.navigate(self.marionette.absolute_url("http_auth"))
        self.wait_for_alert(timeout=self.marionette.timeout.page_load)

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

        self.wait_for_alert_closed()

        status = Wait(self.marionette, timeout=self.marionette.timeout.page_load).until(
            element_present(By.ID, "status")
        )
        self.assertEqual(status.text, "restricted")

    def test_alert_opened_before_session_starts(self):
        self.marionette.navigate(self.marionette.absolute_url("http_auth"))
        self.wait_for_alert(timeout=self.marionette.timeout.page_load)

        # Restart the session to ensure we still find the formerly left-open dialog.
        self.marionette.delete_session()
        self.marionette.start_session()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

        self.wait_for_alert_closed()
