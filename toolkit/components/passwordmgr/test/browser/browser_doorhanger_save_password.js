/**
 * Test that the doorhanger notification for password saving is populated with
 * the correct values in various password capture cases.
 */

const testCases = [
  {
    name: "No saved logins, username and password",
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
    name: "No saved logins, password with empty username",
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
    name: "Saved login with username, update password",
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
    name: "Saved login with no username, update password",
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
    name: "Saved login with no username, add username and different password",
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

for (let testData of testCases) {
  let tmp = {
    async [testData.name]() {
      info("testing with: " + JSON.stringify(testData));
      await test_save_change(testData);
    },
  };
  add_task(tmp[testData.name]);
}

async function test_save_change(testData) {
  let {
    oldUsername,
    username,
    oldPassword,
    password,
    expectOutcome,
  } = testData;
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
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      // Update the form with credentials from the test case.
      info(`update form with username: ${username}, password: ${password}`);
      await changeContentFormValues(browser, {
        "#form-basic-username": username,
        "#form-basic-password": password,
      });

      // Submit the form with the new credentials. This will cause the doorhanger
      // notification to be displayed.
      let formSubmittedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async function() {
        let doc = this.content.document;
        doc.getElementById("form-basic").submit();
      });
      await formSubmittedPromise;

      // Simulate the action on the notification to request the login to be
      // saved, and wait for the data to be updated or saved based on the type
      // of operation we expect.
      let expectedNotification, expectedDoorhanger;
      if (oldPassword !== undefined && oldUsername !== undefined) {
        expectedNotification = "addLogin";
        expectedDoorhanger = "password-save";
      } else if (oldPassword !== undefined) {
        expectedNotification = "modifyLogin";
        expectedDoorhanger = "password-change";
      } else {
        expectedNotification = "addLogin";
        expectedDoorhanger = "password-save";
      }

      info("Waiting for doorhanger of type: " + expectedDoorhanger);
      let notif = await waitForDoorhanger(browser, expectedDoorhanger);

      // Check the actual content of the popup notification.
      await checkDoorhangerUsernamePassword(username, password);

      let promiseLogin = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == expectedNotification
      );
      await clickDoorhangerButton(notif, REMEMBER_BUTTON);
      await promiseLogin;
      await cleanupDoorhanger(notif); // clean slate for the next test

      // Check that the values in the database match the expected values.
      verifyLogins(expectOutcome);
    }
  );

  // Clean up the database before the next test case is executed.
  Services.logins.removeAllUserFacingLogins();
}
