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
                                                       "popupshown",
                                                       (event) => event.target == PopupNotifications.panel);
      yield ContentTask.spawn(browser, [username, password],
        function* ([contentUsername, contentPassword]) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = contentUsername;
          doc.getElementById("form-basic-password").value = contentPassword;
          doc.getElementById("form-basic").submit();
        });
      yield promiseShown;
      let notificationElement = PopupNotifications.panel.childNodes[0];
      // Style flush to make sure binding is attached
      notificationElement.querySelector("#password-notification-password").clientTop;

      // Check the actual content of the popup notification.
      Assert.equal(notificationElement.querySelector("#password-notification-username")
                           .value, username);
      Assert.equal(notificationElement.querySelector("#password-notification-password")
                           .value, password);

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
