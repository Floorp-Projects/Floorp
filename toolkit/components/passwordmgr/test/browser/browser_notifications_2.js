/**
 * Test that the doorhanger main action button is disabled
 * when the password field is empty.
 *
 * Also checks that submiting an empty password throws an error.
 */
add_task(function* test_empty_password() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, function* (browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      yield ContentTask.spawn(browser, null,
        function* () {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = "username";
          doc.getElementById("form-basic-password").value = "p";
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");
      let toggleCheckbox = notificationElement.querySelector("#password-notification-visibilityToggle");

      // Synthesize input to empty the field
      passwordTextbox.focus();
      yield EventUtils.synthesizeKey("VK_RIGHT", {});
      yield EventUtils.synthesizeKey("VK_BACK_SPACE", {});

      let mainActionButton = document.getAnonymousElementByAttribute(notificationElement.button, "anonid", "button");
      Assert.ok(mainActionButton.disabled, "Main action button is disabled");

      // Makes sure submiting an empty password throws an error
      Assert.throws(notificationElement.button.doCommand(),
                    "Can't add a login with a null or empty password.",
                    "Should fail for an empty password");
    });
});

/**
 * Test that the doorhanger password field shows plain or * text
 * when the checkbox is checked.
 *
 */
add_task(function* test_toggle_password() {
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, function* (browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      yield ContentTask.spawn(browser, null,
        function* () {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = "username";
          doc.getElementById("form-basic-password").value = "p";
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");
      let toggleCheckbox = notificationElement.querySelector("#password-notification-visibilityToggle");

      yield EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(toggleCheckbox.checked);
      Assert.equal(passwordTextbox.type, "", "Password textbox changed to plain text");

      yield EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
      Assert.ok(!toggleCheckbox.checked);
      Assert.equal(passwordTextbox.type, "password", "Password textbox changed to * text");
    });
});
