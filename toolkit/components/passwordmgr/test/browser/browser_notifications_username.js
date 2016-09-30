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
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);
      yield ContentTask.spawn(browser, testCase.usernameInPage,
        function* (usernameInPage) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = usernameInPage;
          doc.getElementById("form-basic-password").value = "password";
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
