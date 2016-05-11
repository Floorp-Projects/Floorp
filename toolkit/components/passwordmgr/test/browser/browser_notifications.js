/**
 * Test that the doorhanger notification for password saving is populated with
 * the correct values in various password capture cases.
 */
add_task(function* test_save_change() {
  let testCases = [{
    username: "username",
    password: "password",
  }, {
    username: "",
    password: "password",
  }, {
    username: "username",
    oldPassword: "password",
    password: "newPassword",
  }, {
    username: "",
    oldPassword: "password",
    password: "newPassword",
  }];

  for (let { username, oldPassword, password } of testCases) {
    // Add a login for the origin of the form if testing a change notification.
    if (oldPassword) {
      Services.logins.addLogin(LoginTestUtils.testData.formLogin({
        hostname: "https://example.com",
        formSubmitURL: "https://example.com",
        username,
        password: oldPassword,
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
                                                       "Shown");
      yield ContentTask.spawn(browser, { username, password },
        function* ({ username, password }) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = username;
          doc.getElementById("form-basic-password").value = password;
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      // Check the actual content of the popup notification.
      Assert.equal(notificationElement.querySelector("#password-notification-username")
                           .getAttribute("value"), username);
      Assert.equal(notificationElement.querySelector("#password-notification-password")
                           .getAttribute("value"), password);

      // Simulate the action on the notification to request the login to be
      // saved, and wait for the data to be updated or saved based on the type
      // of operation we expect.
      let expectedNotification = oldPassword ? "modifyLogin" : "addLogin";
      let promiseLogin = TestUtils.topicObserved("passwordmgr-storage-changed",
                         (_, data) => data == expectedNotification);
      notificationElement.button.doCommand();
      let [result] = yield promiseLogin;

      // Check that the values in the database match the expected values.
      let login = oldPassword ? result.QueryInterface(Ci.nsIArray)
                                      .queryElementAt(1, Ci.nsILoginInfo)
                              : result.QueryInterface(Ci.nsILoginInfo);
      Assert.equal(login.username, username);
      Assert.equal(login.password, password);
    });

    // Clean up the database before the next test case is executed.
    Services.logins.removeAllLogins();
  }
});

/**
 * Test changing the username inside the doorhanger notification for passwords.
 *
 * We have to test combination of existing and non-existing logins both for
 * the original one from the webpage and the final one used by the dialog.
 *
 * We also check switching to and from empty usernames.
 */
add_task(function* test_edit_username() {
  let testCases = [{
    usernameInPage: "username",
    usernameChangedTo: "newUsername",
  }, {
    usernameInPage: "username",
    usernameInPageExists: true,
    usernameChangedTo: "newUsername",
  }, {
    usernameInPage: "username",
    usernameChangedTo: "newUsername",
    usernameChangedToExists: true,
  }, {
    usernameInPage: "username",
    usernameInPageExists: true,
    usernameChangedTo: "newUsername",
    usernameChangedToExists: true,
  }, {
    usernameInPage: "",
    usernameChangedTo: "newUsername",
  }, {
    usernameInPage: "newUsername",
    usernameChangedTo: "",
  }, {
    usernameInPage: "",
    usernameChangedTo: "newUsername",
    usernameChangedToExists: true,
  }, {
    usernameInPage: "newUsername",
    usernameChangedTo: "",
    usernameChangedToExists: true,
  }];

  for (let testCase of testCases) {
    info("Test case: " + JSON.stringify(testCase));

    // Create the pre-existing logins when needed.
    if (testCase.usernameInPageExists) {
      Services.logins.addLogin(LoginTestUtils.testData.formLogin({
        hostname: "https://example.com",
        formSubmitURL: "https://example.com",
        username: testCase.usernameInPage,
        password: "old password",
      }));
    }

    if (testCase.usernameChangedToExists) {
      Services.logins.addLogin(LoginTestUtils.testData.formLogin({
        hostname: "https://example.com",
        formSubmitURL: "https://example.com",
        username: testCase.usernameChangedTo,
        password: "old password",
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
                                                       "Shown");
      yield ContentTask.spawn(browser, testCase.usernameInPage,
        function* (usernameInPage) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = usernameInPage;
          doc.getElementById("form-basic-password").value = "password";
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      // Modify the username in the dialog if requested.
      if (testCase.usernameChangedTo) {
        notificationElement.querySelector("#password-notification-username")
                .setAttribute("value", testCase.usernameChangedTo);
      }

      // We expect a modifyLogin notification if the final username used by the
      // dialog exists in the logins database, otherwise an addLogin one.
      let expectModifyLogin = testCase.usernameChangedTo
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
      Assert.equal(login.password, "password");
    });

    // Clean up the database before the next test case is executed.
    Services.logins.removeAllLogins();
  }
});

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
                                                       "popupshown");
      yield ContentTask.spawn(browser, testCase,
        function* (testCase) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = testCase.usernameInPage;
          doc.getElementById("form-basic-password").value = testCase.passwordInPage;
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      // Modify the username in the dialog if requested.
      if (testCase.usernameChangedTo) {
        notificationElement.querySelector("#password-notification-username")
                .setAttribute("value", testCase.usernameChangedTo);
      }

      // Modify the password in the dialog if requested.
      if (testCase.passwordChangedTo) {
        notificationElement.querySelector("#password-notification-password")
                .setAttribute("value", testCase.passwordChangedTo);
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
