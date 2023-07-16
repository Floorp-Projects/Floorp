const { FormHistoryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FormHistoryTestUtils.sys.mjs"
);

const usernameFieldName = "user";

async function cleanup() {
  Services.prefs.clearUserPref("signon.rememberSignons");
  Services.logins.removeAllLogins();
  await FormHistoryTestUtils.clear(usernameFieldName);
}

add_setup(async function () {
  await cleanup();
});

add_task(
  async function test_username_not_saved_in_form_history_when_password_manager_enabled() {
    Services.prefs.setBoolPref("signon.rememberSignons", true);

    const storageChangedPromise = TestUtils.topicObserved(
      "passwordmgr-storage-changed",
      (_, data) => data == "addLogin"
    );

    await testSubmittingLoginFormHTTP(
      "subtst_notifications_1.html",
      async () => {
        const notif = await getCaptureDoorhangerThatMayOpen("password-save");
        await clickDoorhangerButton(notif, REMEMBER_BUTTON);
      }
    );

    await storageChangedPromise;

    const loginEntries = (await Services.logins.getAllLogins()).length;
    const historyEntries = await FormHistoryTestUtils.count(usernameFieldName);

    Assert.equal(
      loginEntries,
      1,
      "Username should be saved in password manager"
    );

    Assert.equal(
      historyEntries,
      0,
      "Username should not be saved in form history"
    );
    await cleanup();
  }
);

add_task(
  async function test_username_saved_in_form_history_when_password_manager_disabled() {
    Services.prefs.setBoolPref("signon.rememberSignons", false);

    await testSubmittingLoginFormHTTP("subtst_notifications_1.html");

    const loginEntries = (await Services.logins.getAllLogins()).length;
    const historyEntries = await FormHistoryTestUtils.count(usernameFieldName);

    Assert.equal(
      loginEntries,
      0,
      "Username should not be saved in password manager"
    );

    Assert.equal(historyEntries, 1, "Username should be saved in form history");
    await cleanup();
  }
);
