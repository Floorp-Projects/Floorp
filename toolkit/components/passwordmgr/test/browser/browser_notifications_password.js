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
add_task(function* test_edit_password() {
  let testCases = [{
    usernameInPage: "username",
    passwordInPage: "password",
    passwordChangedTo: "newPassword",
    timesUsed: 1,
  }, {
    usernameInPage: "username",
    usernameInPageExists: true,
    passwordInPage: "password",
    passwordInStorage: "oldPassword",
    passwordChangedTo: "newPassword",
    timesUsed: 2,
  }, {
    usernameInPage: "username",
    usernameChangedTo: "newUsername",
    usernameChangedToExists: true,
    passwordInPage: "password",
    passwordChangedTo: "newPassword",
    timesUsed: 2,
  }, {
    usernameInPage: "username",
    usernameChangedTo: "newUsername",
    usernameChangedToExists: true,
    passwordInPage: "password",
    passwordChangedTo: "password",
    timesUsed: 2,
    checkPasswordNotUpdated: true,
  }, {
    usernameInPage: "newUsername",
    usernameChangedTo: "",
    usernameChangedToExists: true,
    passwordInPage: "password",
    passwordChangedTo: "newPassword",
    timesUsed: 2,
  }];

  for (let testCase of testCases) {
    info("Test case: " + JSON.stringify(testCase));

    // Create the pre-existing logins when needed.
    if (testCase.usernameInPageExists) {
      Services.logins.addLogin(LoginTestUtils.testData.formLogin({
        hostname: "https://example.com",
        formSubmitURL: "https://example.com",
        username: testCase.usernameInPage,
        password: testCase.passwordInStorage,
      }));
    }

    if (testCase.usernameChangedToExists) {
      Services.logins.addLogin(LoginTestUtils.testData.formLogin({
        hostname: "https://example.com",
        formSubmitURL: "https://example.com",
        username: testCase.usernameChangedTo,
        password: testCase.passwordChangedTo,
      }));
    }

    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: "https://example.com/browser/toolkit/components/" +
           "passwordmgr/test/browser/form_basic.html",
    }, function* (browser) {
      // Submit the form in the content page with the credentials from the test
      // case. This will cause the doorhanger notification to be displayed.
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);
      yield ContentTask.spawn(browser, testCase,
        function* (testCase) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = testCase.usernameInPage;
          doc.getElementById("form-basic-password").value = testCase.passwordInPage;
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;
      let notificationElement = PopupNotifications.panel.childNodes[0];
      // Style flush to make sure binding is attached
      notificationElement.querySelector("#password-notification-password").clientTop;

      // Modify the username in the dialog if requested.
      if (testCase.usernameChangedTo) {
        notificationElement.querySelector("#password-notification-username")
                .value = testCase.usernameChangedTo;
      }

      // Modify the password in the dialog if requested.
      if (testCase.passwordChangedTo) {
        notificationElement.querySelector("#password-notification-password")
                .value = testCase.passwordChangedTo;
      }

      // We expect a modifyLogin notification if the final username used by the
      // dialog exists in the logins database, otherwise an addLogin one.
      let expectModifyLogin = typeof testCase.usernameChangedTo !== "undefined"
                              ? testCase.usernameChangedToExists
                              : testCase.usernameInPageExists;

      // Simulate the action on the notification to request the login to be
      // saved, and wait for the data to be updated or saved based on the type
      // of operation we expect.
      let expectedNotification = expectModifyLogin ? "modifyLogin" : "addLogin";
      let promiseLogin = TestUtils.topicObserved("passwordmgr-storage-changed",
                         (_, data) => data == expectedNotification);
      notificationElement.button.doCommand();
      let [result] = yield promiseLogin;

      // Check that the values in the database match the expected values.
      let login = expectModifyLogin ? result.QueryInterface(Ci.nsIArray)
                                            .queryElementAt(1, Ci.nsILoginInfo)
                                    : result.QueryInterface(Ci.nsILoginInfo);

      Assert.equal(login.username, testCase.usernameChangedTo ||
                                   testCase.usernameInPage);
      Assert.equal(login.password, testCase.passwordChangedTo ||
                                   testCase.passwordInPage);

      let meta = login.QueryInterface(Ci.nsILoginMetaInfo);
      Assert.equal(meta.timesUsed, testCase.timesUsed);

      // Check that the password was not updated if the user is empty
      if (testCase.checkPasswordNotUpdated) {
        Assert.ok(meta.timeLastUsed > meta.timeCreated);
        Assert.ok(meta.timeCreated == meta.timePasswordChanged);
      }
    });

    // Clean up the database before the next test case is executed.
    Services.logins.removeAllLogins();
  }
});
