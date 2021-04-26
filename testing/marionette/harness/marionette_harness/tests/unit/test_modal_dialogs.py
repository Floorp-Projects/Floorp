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

    def open_custom_prompt(self, modal_type):
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

                const [ modalType, browsingContextId ] = arguments;

                const modalTypes = {
                  1: Services.prompt.MODAL_TYPE_CONTENT,
                  2: Services.prompt.MODAL_TYPE_TAB,
                  3: Services.prompt.MODAL_TYPE_WINDOW,
                  4: Services.prompt.MODAL_TYPE_INTERNAL_WINDOW,
                }

                const bc = (modalType === 3) ? null : BrowsingContext.get(browsingContextId);
                Services.prompt.alertBC(bc, modalTypes[modalType], "title", "text");
            """,
                script_args=(modal_type, browsing_context_id),
            )

    @parameterized("content", 1)
    @parameterized("tab", 2)
    @parameterized("window", 3)
    @parameterized("internal_window", 4)
    def test_detect_modal_type(self, type):
        self.open_custom_prompt(type)
        self.wait_for_alert()

        self.marionette.switch_to_alert()

        # Restart the session to ensure we still find the formerly left-open dialog.
        self.marionette.delete_session()
        self.marionette.start_session()

        alert = self.marionette.switch_to_alert()
        alert.dismiss()

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
