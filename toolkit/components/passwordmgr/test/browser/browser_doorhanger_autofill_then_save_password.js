/**
 * Test that after we autofill, the site makes changes to the login, and then the
 * user modifies their login, a save/update doorhanger is shown.
 *
 * This is a regression test for Bug 1632405.
 */

const testCases = [
  {
    name: "autofill, then delete u/p, then fill new u/p should show 'save'",
    oldUsername: "oldUsername",
    oldPassword: "oldPassword",
    actions: [
      {
        setUsername: "newUsername",
      },
      {
        setPassword: "newPassword",
      },
    ],
    expectedNotification: "addLogin",
    expectedDoorhanger: "password-save",
  },
  {
    name:
      "autofill, then delete password, then fill new password should show 'update'",
    oldUsername: "oldUsername",
    oldPassword: "oldPassword",
    actions: [
      {
        setPassword: "newPassword",
      },
    ],
    expectedNotification: "modifyLogin",
    expectedDoorhanger: "password-change",
  },
];

for (let testData of testCases) {
  let tmp = {
    async [testData.name]() {
      info("testing with: " + JSON.stringify(testData));
      await test_save_change(testData);
    },
  };
  add_task(tmp[testData.name]);
}

async function test_save_change({
  name,
  oldUsername,
  oldPassword,
  actions,
  expectedNotification,
  expectedDoorhanger,
}) {
  let originalPrefValue = await Services.prefs.getBoolPref(
    "signon.testOnlyUserHasInteractedByPrefValue"
  );
  Services.prefs.setBoolPref(
    "signon.testOnlyUserHasInteractedByPrefValue",
    false
  );

  info("Starting test: " + name);

  LoginTestUtils.addLogin({
    username: oldUsername,
    password: oldPassword,
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/" +
        "passwordmgr/test/browser/form_basic.html",
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      await ContentTask.spawn(
        browser,
        { oldUsername, oldPassword },
        async function awaitAutofill({ oldUsername, oldPassword }) {
          await ContentTaskUtils.waitForCondition(
            () =>
              content.document.querySelector("#form-basic-username").value ==
                oldUsername &&
              content.document.querySelector("#form-basic-password").value ==
                oldPassword,
            "Await and verify autofill"
          );

          info(
            "Triggering a page navigation that is not initiated by the user"
          );
          content.history.replaceState({}, "", "");
        }
      );

      Services.prefs.setBoolPref(
        "signon.testOnlyUserHasInteractedByPrefValue",
        true
      );

      for (let action of actions) {
        info(`As the user, update form with action: ${JSON.stringify(action)}`);
        if (typeof action.setUsername !== "undefined") {
          await changeContentFormValues(browser, {
            "#form-basic-username": action.setUsername,
          });
        }
        if (typeof action.setPassword !== "undefined") {
          await changeContentFormValues(browser, {
            "#form-basic-password": action.setPassword,
          });
        }
      }

      let expectedUsername =
        [...actions]
          .reverse()
          .map(action => action.setUsername)
          .find(username => !!username) ?? oldUsername;
      let expectedPassword =
        [...actions]
          .reverse()
          .map(action => action.setPassword)
          .find(username => !!username) ?? oldPassword;

      await ContentTask.spawn(
        browser,
        { expectedUsername, expectedPassword },
        async function awaitAutofill({ expectedUsername, expectedPassword }) {
          info("Validating updated fields");
          is(
            expectedUsername,
            content.document.querySelector("#form-basic-username").value,
            "Verify username field updated"
          );
          is(
            expectedPassword,
            content.document.querySelector("#form-basic-password").value,
            "Verify password field updated"
          );
        }
      );

      let formSubmittedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async function() {
        let doc = this.content.document;
        doc.getElementById("form-basic").submit();
      });
      await formSubmittedPromise;

      info("Waiting for doorhanger of type: " + expectedDoorhanger);
      let notif = await waitForDoorhanger(browser, expectedDoorhanger);

      await checkDoorhangerUsernamePassword(expectedUsername, expectedPassword);

      let promiseLogin = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == expectedNotification
      );

      await clickDoorhangerButton(notif, REMEMBER_BUTTON);
      await promiseLogin;
      await cleanupDoorhanger(notif); // clean slate for the next test

      Services.prefs.setBoolPref(
        "signon.testOnlyUserHasInteractedByPrefValue",
        originalPrefValue
      );
    }
  );

  // Clean up the database before the next test case is executed.
  Services.logins.removeAllLogins();
}
