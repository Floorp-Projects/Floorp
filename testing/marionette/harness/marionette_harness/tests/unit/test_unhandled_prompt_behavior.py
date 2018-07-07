from __future__ import absolute_import

from marionette_driver import errors
from marionette_driver.marionette import Alert
from marionette_driver.wait import Wait
from marionette_harness import MarionetteTestCase, parameterized


class TestUnhandledPromptBehavior(MarionetteTestCase):

    def setUp(self):
        super(TestUnhandledPromptBehavior, self).setUp()

        self.marionette.delete_session()

    def tearDown(self):
        # Ensure to close a possible remaining tab modal dialog
        try:
            alert = self.marionette.switch_to_alert()
            alert.dismiss()

            Wait(self.marionette).until(lambda _: not self.alert_present)
        except errors.NoAlertPresentException:
            pass

        super(TestUnhandledPromptBehavior, self).tearDown()

    @property
    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except errors.NoAlertPresentException:
            return False

    def perform_user_prompt_check(self, prompt_type, text, expected_result,
                                  expected_close=True, expected_notify=True):
        if prompt_type not in ["alert", "confirm", "prompt"]:
            raise TypeError("Invalid dialog type: {}".format(prompt_type))

        # No need to call resolve() because opening a prompt stops the script
        self.marionette.execute_async_script("""
            window.return_value = null;
            window.return_value = window[arguments[0]](arguments[1]);
        """, script_args=(prompt_type, text))

        if expected_notify:
            with self.assertRaises(errors.UnexpectedAlertOpen):
                self.marionette.title
            # Bug 1469752 - WebDriverError misses optional data property
            # self.assertEqual(ex.data.text, text)
        else:
            self.marionette.title

        self.assertEqual(self.alert_present, not expected_close)

        prompt_result = self.marionette.execute_script(
            "return window.return_value", new_sandbox=False)
        self.assertEqual(prompt_result, expected_result)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", True)
    @parameterized("prompt", "prompt", "")
    def test_accept(self, prompt_type, result):
        self.marionette.start_session({"unhandledPromptBehavior": "accept"})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result,
                                       expected_notify=False)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", True)
    @parameterized("prompt", "prompt", "")
    def test_accept_and_notify(self, prompt_type, result):
        self.marionette.start_session({"unhandledPromptBehavior": "accept and notify"})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", False)
    @parameterized("prompt", "prompt", None)
    def test_dismiss(self, prompt_type, result):
        self.marionette.start_session({"unhandledPromptBehavior": "dismiss"})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result,
                                       expected_notify=False)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", False)
    @parameterized("prompt", "prompt", None)
    def test_dismiss_and_notify(self, prompt_type, result):
        self.marionette.start_session({"unhandledPromptBehavior": "dismiss and notify"})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", None)
    @parameterized("prompt", "prompt", None)
    def test_ignore(self, prompt_type, result):
        self.marionette.start_session({"unhandledPromptBehavior": "ignore"})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result,
                                       expected_close=False)

    @parameterized("alert", "alert", None)
    @parameterized("confirm", "confirm", False)
    @parameterized("prompt", "prompt", None)
    def test_default(self, prompt_type, result):
        self.marionette.start_session({})
        self.perform_user_prompt_check(prompt_type, "foo {}".format(prompt_type), result)
