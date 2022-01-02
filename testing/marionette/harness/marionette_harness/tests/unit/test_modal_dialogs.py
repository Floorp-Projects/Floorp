from __future__ import absolute_import

from marionette_driver.by import By
from marionette_driver.expected import element_present
from marionette_driver import errors
from marionette_driver.marionette import Alert
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, parameterized, WindowManagerMixin


class TestModalDialogs(WindowManagerMixin, MarionetteTestCase):
    def setUp(self):
        super(TestModalDialogs, self).setUp()
        self.new_tab = self.open_tab()
        self.marionette.switch_to_window(self.new_tab)

        self.http_auth_pref = (
            "network.auth.non-web-content-triggered-resources-http-auth-allow"
        )

    def tearDown(self):
        # Ensure to close all possible remaining tab modal dialogs
        try:
            while True:
                alert = self.marionette.switch_to_alert()
                alert.dismiss()
        except errors.NoAlertPresentException:
            pass

        self.close_all_tabs()
        self.close_all_windows()

        super(TestModalDialogs, self).tearDown()

    @property
    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except errors.NoAlertPresentException:
            return False

    def wait_for_alert(self, timeout=None):
        Wait(self.marionette, timeout=timeout).until(lambda _: self.alert_present)

    def open_custom_prompt(self, modal_type, delay=0):
        browsing_context_id = self.marionette.execute_script(
            """
            return window.browsingContext.id;
        """,
            sandbox="system",
        )

        with self.marionette.using_context("chrome"):
            self.marionette.execute_script(
                """
                const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

                const [ modalType, browsingContextId, delay ] = arguments;

                const modalTypes = {
                  1: Services.prompt.MODAL_TYPE_CONTENT,
                  2: Services.prompt.MODAL_TYPE_TAB,
                  3: Services.prompt.MODAL_TYPE_WINDOW,
                  4: Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
                }

                window.setTimeout(() => {
                  Services.prompt.alertBC(
                    BrowsingContext.get(browsingContextId),
                    modalTypes[modalType],
                    "title",
                    "text"
                  );
                }, delay);
            """,
                script_args=(modal_type, browsing_context_id, delay * 1000),
            )

    @parameterized("content", 1)
    @parameterized("tab", 2)
    @parameterized("window", 3)
    @parameterized("internal_window", 4)
    def test_detect_modal_type_in_current_tab_for_type(self, type):
        self.open_custom_prompt(type)
        self.wait_for_alert()

        self.assertTrue(self.alert_present)

        # Restart the session to ensure we still find the formerly left-open dialog.
        self.marionette.delete_session()
        self.marionette.start_session()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    @parameterized("content", 1)
    @parameterized("tab", 2)
    def test_dont_detect_content_and_tab_modal_type_in_another_tab_for_type(self, type):
        self.open_custom_prompt(type, delay=0.25)

        self.marionette.switch_to_window(self.start_tab)
        with self.assertRaises(errors.TimeoutException):
            self.wait_for_alert(2)

        self.marionette.switch_to_window(self.new_tab)
        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    @parameterized("window", 3)
    @parameterized("internal_window", 4)
    def test_detect_window_modal_type_in_another_tab_for_type(self, type):
        self.open_custom_prompt(type, delay=0.25)

        self.marionette.switch_to_window(self.start_tab)
        self.wait_for_alert()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

        self.marionette.switch_to_window(self.new_tab)
        self.assertFalse(self.alert_present)

    @parameterized("window", 3)
    @parameterized("internal_window", 4)
    def test_detect_window_modal_type_in_another_window_for_type(self, type):
        self.new_window = self.open_window()

        self.marionette.switch_to_window(self.new_window)

        self.open_custom_prompt(type, delay=0.25)

        self.marionette.switch_to_window(self.new_tab)
        with self.assertRaises(errors.TimeoutException):
            self.wait_for_alert(2)

        self.marionette.switch_to_window(self.new_window)
        alert = self.marionette.switch_to_alert()
        alert.dismiss()

        self.marionette.switch_to_window(self.new_tab)
        self.assertFalse(self.alert_present)

    def test_http_auth_dismiss(self):
        with self.marionette.using_prefs({self.http_auth_pref: True}):
            self.marionette.navigate(self.marionette.absolute_url("http_auth"))
            self.wait_for_alert(timeout=self.marionette.timeout.page_load)

            alert = self.marionette.switch_to_alert()
            alert.dismiss()

            status = Wait(
                self.marionette, timeout=self.marionette.timeout.page_load
            ).until(element_present(By.ID, "status"))
            self.assertEqual(status.text, "restricted")

    def test_http_auth_send_keys(self):
        with self.marionette.using_prefs({self.http_auth_pref: True}):
            self.marionette.navigate(self.marionette.absolute_url("http_auth"))
            self.wait_for_alert(timeout=self.marionette.timeout.page_load)

            alert = self.marionette.switch_to_alert()
            with self.assertRaises(errors.UnsupportedOperationException):
                alert.send_keys("foo")
