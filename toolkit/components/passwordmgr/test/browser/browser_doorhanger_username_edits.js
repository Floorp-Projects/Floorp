/**
 * Test changing the username inside the doorhanger notification for passwords.
 *
 * We have to test combination of existing and non-existing logins both for
 * the original one from the webpage and the final one used by the dialog.
 *
 * We also check switching to and from empty usernames.
 */
add_task(async function test_edit_username() {
  let testCases = [
    {
      usernameInPage: "username",
      usernameChangedTo: "newUsername",
    },
    {
      usernameInPage: "username",
      usernameInPageExists: true,
      usernameChangedTo: "newUsername",
    },
    {
      usernameInPage: "username",
      usernameChangedTo: "newUsername",
      usernameChangedToExists: true,
    },
    {
      usernameInPage: "username",
      usernameInPageExists: true,
      usernameChangedTo: "newUsername",
      usernameChangedToExists: true,
    },
    {
      usernameInPage: "",
      usernameChangedTo: "newUsername",
    },
    {
      usernameInPage: "newUsername",
      usernameChangedTo: "",
    },
    {
      usernameInPage: "",
      usernameChangedTo: "newUsername",
      usernameChangedToExists: true,
    },
    {
      usernameInPage: "newUsername",
      usernameChangedTo: "",
      usernameChangedToExists: true,
    },
  ];

  for (let testCase of testCases) {
    info("Test case: " + JSON.stringify(testCase));
    // Clean state before the test case is executed.
    await LoginTestUtils.clearData();
    await cleanupDoorhanger();
    await cleanupPasswordNotifications();

    // Create the pre-existing logins when needed.
    if (testCase.usernameInPageExists) {
      Services.logins.addLogin(
        LoginTestUtils.testData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: testCase.usernameInPage,
          password: "old password",
        })
      );
    }

    if (testCase.usernameChangedToExists) {
      Services.logins.addLogin(
        LoginTestUtils.testData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: testCase.usernameChangedTo,
          password: "old password",
        })
      );
    }

    let formFilledPromise = listenForTestNotification("FormProcessed");

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url:
          "https://example.com/browser/toolkit/components/" +
          "passwordmgr/test/browser/form_basic.html",
      },
      async function(browser) {
        await formFilledPromise;
        await initForm(browser, {
          "#form-basic-username": testCase.usernameInPage,
        });

        let passwordEditedPromise = listenForTestNotification(
          "PasswordEditedOrGenerated"
        );
        info("Editing the form");
        await changeContentFormValues(browser, {
          "#form-basic-password": "password",
        });
        info("Waiting for passwordEditedPromise");
        await passwordEditedPromise;

        // reset doorhanger/notifications, we're only interested in the submit outcome
        await cleanupDoorhanger();
        await cleanupPasswordNotifications();
        // reset message cache, we're only interested in the submit outcome
        await clearMessageCache(browser);

        // Submit the form in the content page with the credentials from the test
        // case. This will cause the doorhanger notification to be displayed.
        info("Submitting the form");
        let formSubmittedPromise = listenForTestNotification("FormSubmit");
        let promiseShown = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popupshown",
          event => event.target == PopupNotifications.panel
        );
        await SpecialPowers.spawn(browser, [], async function() {
          content.document.getElementById("form-basic").submit();
        });
        info("Waiting for the submit message");
        await formSubmittedPromise;

        info("Waiting for the doorhanger");
        let notif = await waitForDoorhanger(browser, "any");
        ok(!notif.dismissed, "Doorhanger is not dismissed");
        await promiseShown;

        // Modify the username in the dialog if requested.
        if (testCase.usernameChangedTo !== undefined) {
          await updateDoorhangerInputValues({
            username: testCase.usernameChangedTo,
          });
        }

        // We expect a modifyLogin notification if the final username used by the
        // dialog exists in the logins database, otherwise an addLogin one.
        let expectModifyLogin =
          testCase.usernameChangedTo !== undefined
            ? testCase.usernameChangedToExists
            : testCase.usernameInPageExists;
        // Simulate the action on the notification to request the login to be
        // saved, and wait for the data to be updated or saved based on the type
        // of operation we expect.
        let expectedNotification = expectModifyLogin
          ? "modifyLogin"
          : "addLogin";
        let promiseLogin = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (_, data) => data == expectedNotification
        );
        let promiseHidden = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popuphidden"
        );
        clickDoorhangerButton(notif, CHANGE_BUTTON);
        await promiseHidden;
        info("Waiting for storage changed");
        let [result] = await promiseLogin;

        // Check that the values in the database match the expected values.
        let login = expectModifyLogin
          ? result
              .QueryInterface(Ci.nsIArray)
              .queryElementAt(1, Ci.nsILoginInfo)
          : result.QueryInterface(Ci.nsILoginInfo);
        Assert.equal(
          login.username,
          testCase.usernameChangedTo !== undefined
            ? testCase.usernameChangedTo
            : testCase.usernameInPage
        );
        Assert.equal(login.password, "password");
      }
    );
  }
});

async function initForm(browser, formDefaults = {}) {
  await ContentTask.spawn(browser, formDefaults, async function(
    selectorValues
  ) {
    for (let [sel, value] of Object.entries(selectorValues)) {
      content.document.querySelector(sel).value = value;
    }
  });
}
