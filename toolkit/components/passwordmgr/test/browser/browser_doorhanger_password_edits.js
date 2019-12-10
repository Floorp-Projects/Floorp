/**
 * Test changing the password inside the doorhanger notification for passwords.
 *
 * We check the following cases:
 *   - Editing the password of a new login.
 *   - Editing the password of an existing login.
 *   - Changing both username and password to an existing login.
 *   - Changing the username to an existing login.
 *   - Editing username to an empty one and a new password.
 *
 * If both the username and password matches an already existing login, we should not
 * update it's password, but only it's usage timestamp and count.
 */
add_task(async function test_edit_password() {
  let testCases = [
    {
      description: "No saved logins, update password in doorhanger",
      usernameInPage: "username",
      passwordInPage: "password",
      passwordChangedTo: "newPassword",
      timesUsed: 1,
    },
    {
      description: "Login is saved, update password in doorhanger",
      usernameInPage: "username",
      usernameInPageExists: true,
      passwordInPage: "password",
      passwordInStorage: "oldPassword",
      passwordChangedTo: "newPassword",
      timesUsed: 2,
    },
    {
      description:
        "Change username in doorhanger to match saved login, update password in doorhanger",
      usernameInPage: "username",
      usernameChangedTo: "newUsername",
      usernameChangedToExists: true,
      passwordInPage: "password",
      passwordChangedTo: "newPassword",
      timesUsed: 2,
    },
    {
      description:
        "Change username in doorhanger to match saved login, dont update password in doorhanger",
      usernameInPage: "username",
      usernameChangedTo: "newUsername",
      usernameChangedToExists: true,
      passwordInPage: "password",
      passwordChangedTo: "password",
      timesUsed: 2,
      checkPasswordNotUpdated: true,
    },
    {
      description:
        "Change username and password in doorhanger to match saved empty-username login",
      usernameInPage: "newUsername",
      usernameChangedTo: "",
      usernameChangedToExists: true,
      passwordInPage: "password",
      passwordChangedTo: "newPassword",
      timesUsed: 2,
    },
  ];

  for (let testCase of testCases) {
    info("Test case: " + JSON.stringify(testCase));

    // Create the pre-existing logins when needed.
    if (testCase.usernameInPageExists) {
      Services.logins.addLogin(
        LoginTestUtils.testData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: testCase.usernameInPage,
          password: testCase.passwordInStorage,
        })
      );
    }

    if (testCase.usernameChangedToExists) {
      Services.logins.addLogin(
        LoginTestUtils.testData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: testCase.usernameChangedTo,
          password: testCase.passwordChangedTo,
        })
      );
    }

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url:
          "https://example.com/browser/toolkit/components/" +
          "passwordmgr/test/browser/form_basic.html",
      },
      async function(browser) {
        // Submit the form in the content page with the credentials from the test
        // case. This will cause the doorhanger notification to be displayed.
        let promiseShown = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popupshown",
          event => event.target == PopupNotifications.panel
        );
        await SpecialPowers.spawn(browser, [testCase], async function(
          contentTestCase
        ) {
          let doc = content.document;
          doc
            .getElementById("form-basic-username")
            .setUserInput(contentTestCase.usernameInPage);
          doc
            .getElementById("form-basic-password")
            .setUserInput(contentTestCase.passwordInPage);
          doc.getElementById("form-basic").submit();
        });
        await promiseShown;
        let notificationElement = PopupNotifications.panel.childNodes[0];

        // Modify the username & password in the dialog if requested.
        await updateDoorhangerInputValues({
          username: testCase.usernameChangedTo,
          password: testCase.passwordChangedTo,
        });

        // We expect a modifyLogin notification if the final username used by the
        // dialog exists in the logins database, otherwise an addLogin one.
        let expectModifyLogin =
          typeof testCase.usernameChangedTo !== "undefined"
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
        notificationElement.button.doCommand();

        let [result] = await promiseLogin;
        await promiseHidden;

        // Check that the values in the database match the expected values.
        let login = expectModifyLogin
          ? result
              .QueryInterface(Ci.nsIArray)
              .queryElementAt(1, Ci.nsILoginInfo)
          : result.QueryInterface(Ci.nsILoginInfo);
        let meta = login.QueryInterface(Ci.nsILoginMetaInfo);

        let expectedLogin = {
          username:
            "usernameChangedTo" in testCase
              ? testCase.usernameChangedTo
              : testCase.usernameInPage,
          password:
            "passwordChangedTo" in testCase
              ? testCase.passwordChangedTo
              : testCase.passwordInPage,
          timesUsed: testCase.timesUsed,
        };
        // Check that the password was not updated if the user is empty
        if (testCase.checkPasswordNotUpdated) {
          expectedLogin.usedSince = meta.timeCreated;
          expectedLogin.timeCreated = meta.timePasswordChanged;
        }
        verifyLogins([expectedLogin]);

        await cleanupDoorhanger();
      }
    );

    // Clean up the database before the next test case is executed.
    Services.logins.removeAllLogins();
  }
});
