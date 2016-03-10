Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);
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

      // Check the actual content of the popup notification.
      Assert.equal(document.getElementById("password-notification-username")
                           .getAttribute("value"), username);
      Assert.equal(document.getElementById("password-notification-password")
                           .getAttribute("value"), password);

      // Simulate the action on the notification to request the login to be
      // saved, and wait for the data to be updated or saved based on the type
      // of operation we expect.
      let expectedNotification = oldPassword ? "modifyLogin" : "addLogin";
      let promiseLogin = TestUtils.topicObserved("passwordmgr-storage-changed",
                         (_, data) => data == expectedNotification);
      let notificationElement = PopupNotifications.panel.childNodes[0];
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

      // Modify the username in the dialog if requested.
      if (testCase.usernameChangedTo) {
        document.getElementById("password-notification-username")
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
      let notificationElement = PopupNotifications.panel.childNodes[0];
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
 *   - Changing the username and password to an existing login with different password.
 *   - Changing the username and password to an existing login with the same password;
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
    timesUsed: 1,
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

      // Modify the username in the dialog if requested.
      if (testCase.usernameChangedTo) {
        document.getElementById("password-notification-username")
                .setAttribute("value", testCase.usernameChangedTo);
      }

      // Modify the password in the dialog if requested.
      if (testCase.passwordChangedTo) {
        document.getElementById("password-notification-password")
                .setAttribute("value", testCase.passwordChangedTo);
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
      let notificationElement = PopupNotifications.panel.childNodes[0];
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

/**
 * Test that the doorhanger main action button is disabled
 * when the password field is empty.
 *
 * Also checks that submiting an empty password throws an error.
 *
 * We skip Linux for now because focusing elements on the doorhanger
 * doesn't work as expected (Bug 1182296)
 */
add_task(function* test_empty_password() {
  if (Services.appinfo.OS == "Linux") {
    Assert.ok(true, "Skipping test on Linux.");
    return;
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

      // Focus the password textbox
      let focusPassword = BrowserTestUtils.waitForEvent(passwordTextbox, "focus");
      passwordTextbox.focus();
      yield focusPassword;

      // Wait for the textbox type to change
      yield ContentTaskUtils.waitForCondition(() => passwordTextbox.type == "", "Password textbox changed type");

      // Synthesize input to empty the field
      EventUtils.synthesizeKey("VK_RIGHT", {});
      yield EventUtils.synthesizeKey("VK_BACK_SPACE", {});

      let mainActionButton = document.getAnonymousElementByAttribute(notificationElement.button, "anonid", "button");

      // Wait for main button to get disabled
      yield ContentTaskUtils.waitForCondition(() => mainActionButton.disabled, "Main action button is disabled");

      // Makes sure submiting an empty password throws an error
      Assert.throws(notificationElement.button.doCommand(),
                    "Can't add a login with a null or empty password.",
                    "Should fail for an empty password");
    });
});

/**
 * Checks that clicking at the doorhanger pane takes the focus
 * out of the password field.
 *
 * We skip Linux for now because focusing elements on the doorhanger
 * doesn't work as expected (Bug 1182296)
 */
add_task(function* test_unfocus_click() {
  if (Services.appinfo.OS == "Linux") {
    Assert.ok(true, "Skipping test on Linux.");
    return;
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
      yield ContentTask.spawn(browser, null,
        function* () {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = "username";
          doc.getElementById("form-basic-password").value = "password";
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector("#password-notification-password");

      // Focus the password textbox
      let focusPassword = BrowserTestUtils.waitForEvent(passwordTextbox, "focus");
      passwordTextbox.focus();
      yield focusPassword;

      // Wait for the textbox type to change
      yield ContentTaskUtils.waitForCondition(() => passwordTextbox.type == "",
                                              "Password textbox changed type");

      let notificationIcon = document.getAnonymousElementByAttribute(notificationElement,
                                                                     "class",
                                                                     "popup-notification-icon");

      yield EventUtils.synthesizeMouseAtCenter(notificationIcon, {});

      // Wait for the textbox type to change back
      yield ContentTaskUtils.waitForCondition(() => passwordTextbox.type == "password",
                                              "Password textbox changed type back to password");
    });
});
