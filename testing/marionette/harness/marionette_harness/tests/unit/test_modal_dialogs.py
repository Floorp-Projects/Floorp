# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette_driver.by import By
from marionette_driver.errors import NoAlertPresentException, ElementNotVisibleException
from marionette_driver.marionette import Alert
from marionette_driver.wait import Wait

from marionette_harness import MarionetteTestCase, skip_if_e10s


class TestTabModals(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs({"prompts.tab_modal.enabled": True})
        self.marionette.navigate(self.marionette.absolute_url('modal_dialogs.html'))

    def alert_present(self):
        try:
            Alert(self.marionette).text
            return True
        except NoAlertPresentException:
            return False

    def tearDown(self):
        # Ensure an alert is absent before proceeding past this test.
        Wait(self.marionette).until(lambda _: not self.alert_present())
        self.marionette.execute_script("window.onbeforeunload = null;")

    def wait_for_alert(self):
        Wait(self.marionette).until(lambda _: self.alert_present())

    def test_no_alert_raises(self):
        self.assertRaises(NoAlertPresentException, Alert(self.marionette).accept)
        self.assertRaises(NoAlertPresentException, Alert(self.marionette).dismiss)

    def test_alert_accept(self):
        self.marionette.find_element(By.ID, 'modal-alert').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()

    def test_alert_dismiss(self):
        self.marionette.find_element(By.ID, 'modal-alert').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()

    def test_confirm_accept(self):
        self.marionette.find_element(By.ID, 'modal-confirm').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'confirm-result').text == 'true')

    def test_confirm_dismiss(self):
        self.marionette.find_element(By.ID, 'modal-confirm').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'confirm-result').text == 'false')

    def test_prompt_accept(self):
        self.marionette.find_element(By.ID, 'modal-prompt').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.accept()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'prompt-result').text == '')

    def test_prompt_dismiss(self):
        self.marionette.find_element(By.ID, 'modal-prompt').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.dismiss()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'prompt-result').text == 'null')

    def test_alert_text(self):
        with self.assertRaises(NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, 'modal-alert').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, 'Marionette alert')
        alert.accept()

    def test_prompt_text(self):
        with self.assertRaises(NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, 'modal-prompt').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, 'Marionette prompt')
        alert.accept()

    def test_confirm_text(self):
        with self.assertRaises(NoAlertPresentException):
            alert = self.marionette.switch_to_alert()
            alert.text
        self.marionette.find_element(By.ID, 'modal-confirm').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertEqual(alert.text, 'Marionette confirm')
        alert.accept()

    def test_set_text_throws(self):
        self.assertRaises(NoAlertPresentException, Alert(self.marionette).send_keys, "Foo")
        self.marionette.find_element(By.ID, 'modal-alert').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        self.assertRaises(ElementNotVisibleException, alert.send_keys, "Foo")
        alert.accept()

    def test_set_text_accept(self):
        self.marionette.find_element(By.ID, 'modal-prompt').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Some text!");
        alert.accept()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'prompt-result').text == 'Some text!')

    def test_set_text_dismiss(self):
        self.marionette.find_element(By.ID, 'modal-prompt').click()
        self.wait_for_alert()
        alert = self.marionette.switch_to_alert()
        alert.send_keys("Some text!");
        alert.dismiss()
        self.wait_for_condition(lambda mn: mn.find_element(By.ID, 'prompt-result').text == 'null')

    def test_onbeforeunload_dismiss(self):
        start_url = self.marionette.get_url()
        self.marionette.find_element(By.ID, 'onbeforeunload-handler').click()
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
        self.marionette.find_element(By.ID, 'onbeforeunload-handler').click()
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

    @skip_if_e10s("Bug 1325044")
    def test_unrelated_command_when_alert_present(self):
        click_handler = self.marionette.find_element(By.ID, 'click-handler')
        text = self.marionette.find_element(By.ID, 'click-result').text
        self.assertEqual(text, '')

        self.marionette.find_element(By.ID, 'modal-alert').click()
        self.wait_for_alert()

        # Commands succeed, but because the dialog blocks the event loop,
        # our actions aren't reflected on the page.
        text = self.marionette.find_element(By.ID, 'click-result').text
        self.assertEqual(text, '')
        click_handler.click()
        text = self.marionette.find_element(By.ID, 'click-result').text
        self.assertEqual(text, '')

        alert = self.marionette.switch_to_alert()
        alert.accept()

        Wait(self.marionette).until(lambda _: not self.alert_present())

        click_handler.click()
        text = self.marionette.find_element(By.ID, 'click-result').text
        self.assertEqual(text, 'result')


class TestGlobalModals(TestTabModals):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.enforce_gecko_prefs({"prompts.tab_modal.enabled": False})
        self.marionette.navigate(self.marionette.absolute_url('modal_dialogs.html'))

    def test_unrelated_command_when_alert_present(self):
        # The assumptions in this test do not hold on certain platforms, and not when
        # e10s is enabled.
        pass
