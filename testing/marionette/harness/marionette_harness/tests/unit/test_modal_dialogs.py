from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.expected import element_present
from marionette_driver import errors
from marionette_driver.marionette import Alert
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, parameterized, WindowManagerMixin


class BaseAlertTestCase(WindowManagerMixin, MarionetteTestCase):

    @property
    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except errors.NoAlertPresentException:
            return False

    def wait_for_alert(self, timeout=None):
        Wait(self.marionette, timeout=timeout).until(
            lambda _: self.alert_present)


class TestTabModalAlerts(BaseAlertTestCase):

    def setUp(self):
        super(TestTabModalAlerts, self).setUp()
        self.assertTrue(self.marionette.get_pref("prompts.tab_modal.enabled",
                        "Tab modal alerts should be enabled by default."))

        self.test_page = self.marionette.absolute_url("test_tab_modal_dialogs.html")
        self.marionette.navigate(self.test_page)

    def tearDown(self):
        # Ensure to close all possible remaining tab modal dialogs
        try:
            while True:
                alert = self.marionette.switch_to_alert()
                alert.dismiss()
        except errors.NoAlertPresentException:
            pass

        super(TestTabModalAlerts, self).tearDown()

    def test_no_alert_raises(self):
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).accept()
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).dismiss()

    def test_alert_opened_before_session_starts(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()

        # Restart the session to ensure we still find the formerly left-open dialog.
        self.marionette.delete_session()
        self.marionette.start_session()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    @parameterized("alert", "alert", "undefined")
    @parameterized("confirm", "confirm", "true")
    @parameterized("prompt", "prompt", "")
    def test_accept(self, value, result):
        self.marionette.find_element(By.ID, "tab-modal-{}".format(value)).click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()
        self.assertEqual(self.marionette.find_element(By.ID, "text").text, result)

    @parameterized("alert", "alert", "undefined")
    @parameterized("confirm", "confirm", "false")
    @parameterized("prompt", "prompt", "null")
    def test_dismiss(self, value, result):
        self.marionette.find_element(By.ID, "tab-modal-{}".format(value)).click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()
        self.assertEqual(self.marionette.find_element(By.ID, "text").text, result)

    @parameterized("alert", "alert", "Marionette alert")
    @parameterized("confirm", "confirm", "Marionette confirm")
    @parameterized("prompt", "prompt", "Marionette prompt")
    def test_text(self, value, text):
        with self.assertRaises(errors.NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, "tab-modal-{}".format(value)).click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, text)
        alert.accept()

    @parameterized("alert", "alert")
    @parameterized("confirm", "confirm")
    def test_set_text_throws(self, value):
        with self.assertRaises(errors.NoAlertPresentException):
            Alert(self.marionette).send_keys("Foo")
        self.marionette.find_element(By.ID, "tab-modal-{}".format(value)).click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        with self.assertRaises(errors.ElementNotInteractableException):
            alert.send_keys("Foo")
        alert.accept()

    def test_set_text_accept(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Foo bar")
        alert.accept()
        self.assertEqual(self.marionette.find_element(By.ID, "text").text, "Foo bar")

    def test_set_text_dismiss(self):
        self.marionette.find_element(By.ID, "tab-modal-prompt").click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Some text!")
        alert.dismiss()
        self.assertEqual(self.marionette.find_element(By.ID, "text").text, "null")

    def test_unrelated_command_when_alert_present(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        with self.assertRaises(errors.UnexpectedAlertOpen):
            self.marionette.find_element(By.ID, "text")

    def test_modal_is_dismissed_after_unexpected_alert(self):
        self.marionette.find_element(By.ID, "tab-modal-alert").click()
        self.wait_for_alert()
        with self.assertRaises(errors.UnexpectedAlertOpen):
            self.marionette.find_element(By.ID, "text")

        assert not self.alert_present

    def test_handle_two_modal_dialogs(self):
        self.marionette.find_element(By.ID, "open-two-dialogs").click()

        self.wait_for_alert()
        alert1 = self.marionette.switch_to_alert()
        alert1.send_keys("foo")
        alert1.accept()

        alert2 = self.marionette.switch_to_alert()
        alert2.send_keys("bar")
        alert2.accept()

        self.assertEqual(self.marionette.find_element(By.ID, "text1").text, "foo")
        self.assertEqual(self.marionette.find_element(By.ID, "text2").text, "bar")


class TestModalAlerts(BaseAlertTestCase):

    def setUp(self):
        super(TestModalAlerts, self).setUp()
        self.marionette.set_pref(
            "network.auth.non-web-content-triggered-resources-http-auth-allow",
            True)

    def tearDown(self):
        # Ensure to close a possible remaining modal dialog
        self.close_all_windows()
        self.marionette.clear_pref(
            "network.auth.non-web-content-triggered-resources-http-auth-allow")

        super(TestModalAlerts, self).tearDown()

    def test_http_auth_dismiss(self):
        self.marionette.navigate(self.marionette.absolute_url("http_auth"))
        self.wait_for_alert(timeout=self.marionette.timeout.page_load)

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

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
