add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["signon.rememberSignons.visibilityToggle", true]
  ]});
});

/**
 * Test that the doorhanger main action button is disabled
 * when the password field is empty.
 *
 * Also checks that submiting an empty password throws an error.
 */
add_task(async function test_empty_password() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);
      await ContentTask.spawn(browser, null,
        async function() {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = "username";
          doc.getElementById("form-basic-password").value = "p";
          doc.getElementById("form-basic").submit();
        });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");
      let toggleCheckbox = notificationElement.querySelector("#password-notification-visibilityToggle");

      // Synthesize input to empty the field
      passwordTextbox.focus();
      await EventUtils.synthesizeKey("KEY_ArrowRight");
      await EventUtils.synthesizeKey("KEY_Backspace");

      let mainActionButton = notificationElement.button;
      Assert.ok(mainActionButton.disabled, "Main action button is disabled");
    });
});

/**
 * Test that the doorhanger password field shows plain or * text
 * when the checkbox is checked.
 */
add_task(async function test_toggle_password() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);
      await ContentTask.spawn(browser, null,
        async function() {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = "username";
          doc.getElementById("form-basic-password").value = "p";
          doc.getElementById("form-basic").submit();
        });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");
      let toggleCheckbox = notificationElement.querySelector("#password-notification-visibilityToggle");

      await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(toggleCheckbox.checked);
      Assert.equal(passwordTextbox.type, "", "Password textbox changed to plain text");

      await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(!toggleCheckbox.checked);
      Assert.equal(passwordTextbox.type, "password", "Password textbox changed to * text");
    });
});

/**
 * Test that the doorhanger password toggle checkbox is disabled
 * when the master password is set.
 */
add_task(async function test_checkbox_disabled_if_has_master_password() {
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, async function(browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);

      LoginTestUtils.masterPassword.enable();

      await ContentTask.spawn(browser, null, async function() {
        let doc = content.document;
        doc.getElementById("form-basic-username").value = "username";
        doc.getElementById("form-basic-password").value = "p";
        doc.getElementById("form-basic").submit();
      });
      await promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");
      let toggleCheckbox = notificationElement.querySelector("#password-notification-visibilityToggle");

      Assert.equal(passwordTextbox.type, "password", "Password textbox should show * text");
      Assert.ok(toggleCheckbox.getAttribute("hidden"), "checkbox is hidden when master password is set");
    });

  LoginTestUtils.masterPassword.disable();
});
