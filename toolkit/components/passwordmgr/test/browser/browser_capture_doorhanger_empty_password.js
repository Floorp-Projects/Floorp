add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons.visibilityToggle", true]],
  });
});

/**
 * Test that the doorhanger main action button is disabled
 * when the password field is empty.
 *
 * Also checks that submiting an empty password throws an error.
 */
add_task(async function test_empty_password() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/passwordmgr/test/browser/form_basic.html",
    },
    async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown",
        event => event.target == PopupNotifications.panel
      );
      await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        doc.getElementById("form-basic-username").value = "username";
        doc.getElementById("form-basic-password").value = "pw";
        doc.getElementById("form-basic").submit();
      });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector(
        "#password-notification-password"
      );

      // Synthesize input to empty the field
      passwordTextbox.focus();
      await EventUtils.synthesizeKey("KEY_ArrowRight");
      await EventUtils.synthesizeKey("KEY_ArrowRight");
      await EventUtils.synthesizeKey("KEY_Backspace");
      await EventUtils.synthesizeKey("KEY_Backspace");

      let mainActionButton = notificationElement.button;
      Assert.ok(mainActionButton.disabled, "Main action button is disabled");
    }
  );
});
