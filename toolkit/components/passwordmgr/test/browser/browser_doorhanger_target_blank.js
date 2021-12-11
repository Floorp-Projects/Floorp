/**
 * Test capture popup notifications when the login form uses target="_blank"
 */

add_task(async function setup() {
  await SimpleTest.promiseFocus(window);
});

add_task(async function test_saveTargetBlank() {
  // This test submits the form to a new tab using target="_blank".
  let url = "subtst_notifications_12_target_blank.html?notifyu3|notifyp3||";
  let notifShownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  let submissionTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => {
      info(url);
      return url.includes("formsubmit.sjs");
    },
    false,
    true
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://mochi.test:8888" + DIRECTORY_PATH + url,
    },
    async function() {
      // For now the doorhanger appears in the previous tab but it should maybe
      // appear in the new tab from target="_blank"?
      BrowserTestUtils.removeTab(await submissionTabPromise);

      let notif = await TestUtils.waitForCondition(
        () =>
          getCaptureDoorhangerThatMayOpen(
            "password-save",
            PopupNotifications,
            gBrowser.selectedBrowser
          ),
        "Waiting for doorhanger"
      );
      ok(notif, "got notification popup");

      EventUtils.synthesizeMouseAtCenter(notif.anchorElement, {});
      await notifShownPromise;
      await checkDoorhangerUsernamePassword("notifyu3", "notifyp3");
      let storageChangedPromised = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (subject, data) => data != "removeLogin"
      );

      clickDoorhangerButton(notif, REMEMBER_BUTTON);
      await storageChangedPromised;
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  );

  // Check result of clicking Remember
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login now");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.username, "notifyu3", "Check the username used on the new entry");
  is(login.password, "notifyp3", "Check the password used on the new entry");
  is(login.timesUsed, 1, "Check times used on new entry");

  // Check for stale values in the doorhanger <input> after closing.
  let usernameField = document.getElementById("password-notification-username");
  todo_is(
    usernameField.value,
    "",
    "Check the username field doesn't have a stale value"
  );
  let passwordField = document.getElementById("password-notification-password");
  todo_is(
    passwordField.value,
    "",
    "Check the password field doesn't have a stale value"
  );

  // Cleanup
  Services.logins.removeLogin(login);
});
