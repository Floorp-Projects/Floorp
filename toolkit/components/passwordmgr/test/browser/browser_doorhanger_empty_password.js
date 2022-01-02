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
      await SpecialPowers.spawn(browser, [], async function() {
        let doc = content.document;
        doc.getElementById("form-basic-username").setUserInput("username");
        doc.getElementById("form-basic-password").setUserInput("pw");
        doc.getElementById("form-basic").submit();
      });

      await waitForDoorhanger(browser, "password-save");
      // Synthesize input to empty the field
      await updateDoorhangerInputValues({
        password: "",
      });

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let mainActionButton = notificationElement.button;

      Assert.ok(mainActionButton.disabled, "Main action button is disabled");
      await hideDoorhangerPopup();
    }
  );
});
