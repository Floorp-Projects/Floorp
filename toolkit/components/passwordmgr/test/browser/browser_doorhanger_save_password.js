/**
 * Test that the doorhanger notification for password saving is populated with
 * the correct values in various password capture cases.
 */
add_task(async function test_save_change() {
  let testCases = [
    {
      username: "username",
      password: "password",
      expectOutcome: [
        {
          username: "username",
          password: "password",
        },
      ],
    },
    {
      username: "",
      password: "password",
      expectOutcome: [
        {
          username: "",
          password: "password",
        },
      ],
    },
    {
      username: "username",
      oldPassword: "password",
      password: "newPassword",
      expectOutcome: [
        {
          username: "username",
          password: "newPassword",
        },
      ],
    },
    {
      username: "",
      oldPassword: "password",
      password: "newPassword",
      expectOutcome: [
        {
          username: "",
          password: "newPassword",
        },
      ],
    },
    {
      oldUsername: "",
      username: "username",
      oldPassword: "password",
      password: "newPassword",
      expectOutcome: [
        {
          username: "",
          password: "password",
        },
        {
          username: "username",
          password: "newPassword",
        },
      ],
    },
  ];

  for (let {
    oldUsername,
    username,
    oldPassword,
    password,
    expectOutcome,
  } of testCases) {
    // Add a login for the origin of the form if testing a change notification.
    if (oldPassword) {
      Services.logins.addLogin(
        LoginTestUtils.testData.formLogin({
          origin: "https://example.com",
          formActionOrigin: "https://example.com",
          username: typeof oldUsername !== "undefined" ? oldUsername : username,
          password: oldPassword,
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
        await ContentTask.spawn(browser, [username, password], async function([
          contentUsername,
          contentPassword,
        ]) {
          let doc = content.document;
          doc.getElementById("form-basic-username").value = contentUsername;
          doc.getElementById("form-basic-password").value = contentPassword;
          doc.getElementById("form-basic").submit();
        });
        await promiseShown;
        let notif = PopupNotifications.getNotification("password", browser);
        let notificationElement = PopupNotifications.panel.childNodes[0];
        // Style flush to make sure binding is attached
        notificationElement.querySelector("#password-notification-password")
          .clientTop;

        // Check the actual content of the popup notification.
        Assert.equal(
          notificationElement.querySelector("#password-notification-username")
            .value,
          username
        );
        Assert.equal(
          notificationElement.querySelector("#password-notification-password")
            .value,
          password
        );

        // Simulate the action on the notification to request the login to be
        // saved, and wait for the data to be updated or saved based on the type
        // of operation we expect.
        let expectedNotification;
        if (oldPassword !== undefined && oldUsername !== undefined) {
          expectedNotification = "addLogin";
        } else if (oldPassword !== undefined) {
          expectedNotification = "modifyLogin";
        } else {
          expectedNotification = "addLogin";
        }
        let promiseLogin = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (_, data) => data == expectedNotification
        );
        notificationElement.button.doCommand();
        await promiseLogin;
        await cleanupDoorhanger(notif); // clean slate for the next test

        // Check that the values in the database match the expected values.
        verifyLogins(expectOutcome);
      }
    );

    // Clean up the database before the next test case is executed.
    Services.logins.removeAllLogins();
  }
});
