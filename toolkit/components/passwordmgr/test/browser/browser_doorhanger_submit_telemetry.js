/**
 * Test that doorhanger submit telemetry is sent when the user saves/updates.
 */

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
          did_edit_pw: "false",
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
        doorhangerChanges: {
          username: "doorhangerUn",
        },
      },
      {
        pageChanges: { password: "pagePw2" },
        doorhangerChanges: {
          password: "doorhangerPw",
        },
      },
    ],
    expectedEvents: [
      {
        type: "save",
        ping: {
          did_edit_un: "true",
          did_edit_pw: "false",
        },
      },
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_edit_pw: "true",
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
        doorhangerChanges: {
          password: "doorhangerPw",
        },
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "false",
          did_edit_pw: "true",
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
        doorhangerChanges: {
          username: "doorhangerUn",
        },
      },
    ],
    expectedEvents: [
      {
        type: "update",
        ping: {
          did_edit_un: "true",
          did_edit_pw: "false",
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

async function test_submit_telemetry(tc) {
  if (tc.savedLogin) {
    Services.logins.addLogin(
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
      async function(browser) {
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

        let formSubmittedPromise = listenForTestNotification("FormSubmit");
        await SpecialPowers.spawn(browser, [], async function() {
          let doc = this.content.document;
          doc.getElementById("form-basic").submit();
        });
        await formSubmittedPromise;

        let saveDoorhanger = waitForDoorhanger(browser, "password-save");
        let updateDoorhanger = waitForDoorhanger(browser, "password-change");
        info("Waiting for doorhanger");
        notif = await Promise.race([saveDoorhanger, updateDoorhanger]);

        await updateDoorhangerInputValues(userAction.doorhangerChanges);

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
  Services.logins.removeAllLogins();
}
