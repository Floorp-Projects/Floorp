/**
 * Test that doorhanger submit telemetry is sent when the user saves/updates.
 */

add_setup(function () {
  // This test used to rely on the initial timer of
  // TestUtils.waitForCondition. See bug 1695395.
  // The test is perma-fail on Linux asan opt without this.
  let originalWaitForCondition = TestUtils.waitForCondition;
  TestUtils.waitForCondition = async function (
    condition,
    msg,
    interval = 100,
    maxTries = 50
  ) {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));

    return originalWaitForCondition(condition, msg, interval, maxTries);
  };
  registerCleanupFunction(function () {
    TestUtils.waitForCondition = originalWaitForCondition;
  });
});

const PAGE_USERNAME_SELECTOR = "#form-basic-username";
const PAGE_PASSWORD_SELECTOR = "#form-basic-password";

const TEST_CASES = [
  {
    description:
      "Saving a new login from page values without modification sends a 'no modification' event",
    savedLogin: undefined,
    userActions: [
      {
        pageChanges: {
          username: "pageUn",
          password: "pagePw",
        },
        doorhangerChanges: [],
      },
    ],
    expectedEvents: [
      {
        type: "save",
        ping: {
          did_edit_un: "false",
          did_select_un: "false",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
  {
    description: "Saving two logins sends two events",
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            typedUsername: "doorhangerUn",
          },
        ],
      },
      {
        pageChanges: { password: "pagePw2" },
        doorhangerChanges: [
          {
            typedPassword: "doorhangerPw",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "save",
        ping: {
          did_edit_un: "true",
          did_select_un: "false",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_select_un: "false",
          did_edit_pw: "true",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
  {
    description: "Updating a doorhanger password sends a 'pw updated' event",
    savedLogin: {
      username: "savedUn",
      password: "savedPw",
    },
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            typedPassword: "doorhangerPw",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_select_un: "false",
          did_edit_pw: "true",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
  {
    description:
      "Saving a new username with an existing password sends a 'un updated' event",
    savedLogin: {
      username: "savedUn",
      password: "savedPw",
    },
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            typedUsername: "doorhangerUn",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "true",
          did_select_un: "false",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
    ],
  },
  ///////////////
  {
    description: "selecting a saved username sends a 'not edited' event",
    savedLogin: {
      username: "savedUn",
      password: "savedPw",
    },
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            selectUsername: "savedUn",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_select_un: "true",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
  {
    description:
      "typing a new username then selecting a saved username sends a 'not edited' event",
    savedLogin: {
      username: "savedUn",
      password: "savedPw",
    },
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            typedUsername: "doorhangerTypedUn",
          },
          {
            selectUsername: "savedUn",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_select_un: "true",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
  {
    description:
      "selecting a saved username then typing a new username sends an 'edited' event",
    savedLogin: {
      username: "savedUn",
      password: "savedPw",
    },
    userActions: [
      {
        pageChanges: { password: "pagePw" },
        doorhangerChanges: [
          {
            selectUsername: "savedUn",
          },
          {
            typedUsername: "doorhangerTypedUn",
          },
        ],
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "true",
          did_select_un: "false",
          did_edit_pw: "false",
          did_select_pw: "false",
        },
      },
    ],
  },
  /////////////////
];

for (let testData of TEST_CASES) {
  let tmp = {
    async [testData.description]() {
      info("testing with: " + JSON.stringify(testData));
      await test_submit_telemetry(testData);
    },
  };
  add_task(tmp[testData.description]);
}

function _validateTestCase(tc) {
  for (let event of tc.expectedEvents) {
    Assert.ok(
      !(event.ping.did_edit_un && event.ping.did_select_un),
      "'did_edit_un' and 'did_select_un' can never be true at the same time"
    );
    Assert.ok(
      !(event.ping.did_edit_pw && event.ping.did_select_pw),
      "'did_edit_pw' and 'did_select_pw' can never be true at the same time"
    );
  }
}

async function test_submit_telemetry(tc) {
  if (tc.savedLogin) {
    await Services.logins.addLoginAsync(
      LoginTestUtils.testData.formLogin({
        origin: "https://example.com",
        formActionOrigin: "https://example.com",
        username: tc.savedLogin.username,
        password: tc.savedLogin.password,
      })
    );
  }

  let notif;
  for (let userAction of tc.userActions) {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url:
          "https://example.com/browser/toolkit/components/" +
          "passwordmgr/test/browser/form_basic.html",
      },
      async function (browser) {
        await SimpleTest.promiseFocus(browser.ownerGlobal);

        if (userAction.pageChanges) {
          info(
            `Update form with changes: ${JSON.stringify(
              userAction.pageChanges
            )}`
          );
          let changeTo = {};
          if (userAction.pageChanges.username) {
            changeTo[PAGE_USERNAME_SELECTOR] = userAction.pageChanges.username;
          }
          if (userAction.pageChanges.password) {
            changeTo[PAGE_PASSWORD_SELECTOR] = userAction.pageChanges.password;
          }

          await changeContentFormValues(browser, changeTo);
        }

        info("Submitting form");
        let formSubmittedPromise = listenForTestNotification("ShowDoorhanger");
        await SpecialPowers.spawn(browser, [], async function () {
          let doc = this.content.document;
          doc.getElementById("form-basic").submit();
        });
        await formSubmittedPromise;

        let saveDoorhanger = waitForDoorhanger(browser, "password-save");
        let updateDoorhanger = waitForDoorhanger(browser, "password-change");
        notif = await Promise.race([saveDoorhanger, updateDoorhanger]);

        if (PopupNotifications.panel.state !== "open") {
          await BrowserTestUtils.waitForEvent(
            PopupNotifications.panel,
            "popupshown"
          );
        }

        if (userAction.doorhangerChanges) {
          for (let doorhangerChange of userAction.doorhangerChanges) {
            if (
              doorhangerChange.typedUsername ||
              doorhangerChange.typedPassword
            ) {
              await updateDoorhangerInputValues({
                username: doorhangerChange.typedUsername,
                password: doorhangerChange.typedPassword,
              });
            }

            if (doorhangerChange.selectUsername) {
              await selectDoorhangerUsername(doorhangerChange.selectUsername);
            }
            if (doorhangerChange.selectPassword) {
              await selectDoorhangerPassword(doorhangerChange.selectPassword);
            }
          }
        }

        info("Waiting for doorhanger");
        await clickDoorhangerButton(notif, REMEMBER_BUTTON);
      }
    );
  }

  let expectedEvents = tc.expectedEvents.map(expectedEvent => [
    "pwmgr",
    "doorhanger_submitted",
    expectedEvent.type,
    null,
    expectedEvent.ping,
  ]);

  await LoginTestUtils.telemetry.waitForEventCount(
    expectedEvents.length,
    "parent",
    "pwmgr",
    "doorhanger_submitted"
  );
  TelemetryTestUtils.assertEvents(
    expectedEvents,
    { category: "pwmgr", method: "doorhanger_submitted" },
    { clear: true }
  );

  // Clean up the database before the next test case is executed.
  await cleanupDoorhanger(notif);
  Services.logins.removeAllUserFacingLogins();
}
