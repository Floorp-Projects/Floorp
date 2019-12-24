/**
 * Test result of different input to the promptToChangePassword doorhanger
 */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const passwordInputSelector = "#form-basic-password";
const usernameInputSelector = "#form-basic-username";

const availLoginsByValue = new Map();
let savedLoginsByName;
const finalLoginsByGuid = new Map();
let finalLogins;

const availLogins = {
  emptyXYZ: LoginTestUtils.testData.formLogin({
    username: "",
    password: "xyz",
  }),
  bobXYZ: LoginTestUtils.testData.formLogin({
    username: "bob",
    password: "xyz",
  }),
  bobABC: LoginTestUtils.testData.formLogin({
    username: "bob",
    password: "abc",
  }),
};
availLoginsByValue.set(availLogins.emptyXYZ, "emptyXYZ");
availLoginsByValue.set(availLogins.bobXYZ, "bobXYZ");
availLoginsByValue.set(availLogins.bobABC, "bobABC");

async function showChangePasswordDoorhanger(
  browser,
  oldLogin,
  formLogin,
  { notificationType = "password-change", autoSavedLoginGuid = "" } = {}
) {
  let windowGlobal = browser.browsingContext.currentWindowGlobal;
  let loginManagerActor = windowGlobal.getActor("LoginManager");
  let prompter = loginManagerActor._getPrompter(browser, null);
  ok(
    !PopupNotifications.isPanelOpen,
    "Check the doorhanger isn't already open"
  );

  let promiseShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  prompter.promptToChangePassword(
    browser,
    oldLogin,
    formLogin,
    false, // dimissed prompt
    false, // notifySaved
    autoSavedLoginGuid
  );
  await promiseShown;

  let notif = getCaptureDoorhanger(notificationType);
  ok(notif, `${notificationType} notification exists`);

  let { panel } = PopupNotifications;
  let notificationElement = panel.childNodes[0];
  await BrowserTestUtils.waitForCondition(() => {
    return (
      notificationElement.querySelector("#password-notification-password")
        .value == formLogin.password &&
      notificationElement.querySelector("#password-notification-username")
        .value == formLogin.username
    );
  }, "Wait for the notification panel to be populated");
  return notif;
}

async function setupLogins(...logins) {
  Services.logins.removeAllLogins();
  let savedLogins = {};
  let timesCreated = new Set();
  for (let login of logins) {
    let loginName = availLoginsByValue.get(login);
    let savedLogin = await LoginTestUtils.addLogin(login);
    // we rely on sorting by timeCreated so ensure none are identical
    ok(
      !timesCreated.has(savedLogin.timeCreated),
      "Each login has a different timeCreated"
    );
    timesCreated.add(savedLogin.timeCreated);
    savedLogins[loginName || savedLogin.guid] = savedLogin.clone();
  }
  return savedLogins;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.autofillForms", false]],
  });
  ok(!PopupNotifications.isPanelOpen, "No notifications panel open");
});

async function promptToChangePasswordTest(testData) {
  info("Starting: " + testData.name);
  savedLoginsByName = await setupLogins(...testData.initialSavedLogins);
  await SimpleTest.promiseFocus();
  info("got focus");

  let oldLogin = savedLoginsByName[testData.promptArgs.oldLogin];
  let changeLogin = LoginTestUtils.testData.formLogin(
    testData.promptArgs.changeLogin
  );
  let options;
  if (testData.autoSavedLoginName) {
    options = {
      autoSavedLoginGuid: savedLoginsByName[testData.autoSavedLoginName].guid,
    };
  }
  info(
    "Waiting for showChangePasswordDoorhanger, username: " +
      changeLogin.username
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      TEST_ORIGIN,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      let notif = await showChangePasswordDoorhanger(
        browser,
        oldLogin,
        changeLogin,
        options
      );

      await updateDoorhangerInputValues(testData.promptTextboxValues);

      let mainActionButton = getDoorhangerButton(notif, CHANGE_BUTTON);
      is(
        mainActionButton.label,
        testData.expectedButtonLabel,
        "Check button label"
      );

      let { panel } = PopupNotifications;
      let promiseHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
      let storagePromise;
      if (testData.expectedStorageChange) {
        storagePromise = TestUtils.topicObserved("passwordmgr-storage-changed");
      }

      info("Clicking mainActionButton");
      mainActionButton.doCommand();
      info("Waiting for promiseHidden");
      await promiseHidden;
      info("Waiting for storagePromise");
      await storagePromise;

      // ensure the notification was removed to keep clean state for next run
      await cleanupDoorhanger(notif);

      info(testData.resultDescription);

      finalLoginsByGuid.clear();
      finalLogins = Services.logins.getAllLogins();
      finalLogins.sort((a, b) => a.timeCreated > b.timeCreated);

      for (let l of finalLogins) {
        info(`saved login: ${l.guid}: ${l.username}/${l.password}`);
        finalLoginsByGuid.set(l.guid, l);
      }
      info("verifyLogins next");
      verifyLogins(testData.expectedResultLogins);
      if (testData.resultCheck) {
        testData.resultCheck();
      }
    }
  );
}

let tests = [
  {
    name: "Add username to sole login",
    initialSavedLogins: [availLogins.emptyXYZ],
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "zaphod",
        password: "xyz",
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    resultDescription: "The existing login just gets a new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "zaphod",
        password: "xyz",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.emptyXYZ.guid, "Check guid");
    },
  },
  {
    name: "Change password of the sole login",
    initialSavedLogins: [availLogins.bobXYZ],
    promptArgs: {
      oldLogin: "bobXYZ",
      changeLogin: {
        username: "bob",
        password: "&*$",
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    resultDescription: "The existing login just gets a new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "bob",
        password: "&*$",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobXYZ.guid, "Check guid");
    },
  },
  {
    name: "Change password of the sole empty-username login",
    initialSavedLogins: [availLogins.emptyXYZ],
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "",
        password: "&*$",
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    resultDescription: "The existing login just gets a new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "",
        password: "&*$",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.emptyXYZ.guid, "Check guid");
    },
  },
  {
    name: "Add different username to empty-usernamed login",
    initialSavedLogins: [availLogins.emptyXYZ, availLogins.bobABC],
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "alice",
        password: "xyz",
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    resultDescription: "The existing login just gets a new username",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "alice",
        password: "xyz",
      },
      {
        username: "bob",
        password: "abc",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.emptyXYZ.guid, "Check guid");
      ok(
        finalLogins[0].timeLastUsed > savedLoginsByName.emptyXYZ.timeLastUsed,
        "Check timeLastUsed of 0th login"
      );
    },
  },
  {
    name:
      "Add username to autosaved login to match an existing usernamed login",
    initialSavedLogins: [availLogins.emptyXYZ, availLogins.bobABC],
    autoSavedLoginName: "emptyXYZ",
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "bob",
        password: availLogins.emptyXYZ.password,
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    resultDescription:
      "Empty-username login is removed, other login gets the empty-login's password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "bob",
        password: "xyz",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[0].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed changed"
      );
    },
  },
  {
    name:
      "Add username to non-autosaved login to match an existing usernamed login",
    initialSavedLogins: [availLogins.emptyXYZ, availLogins.bobABC],
    autoSavedLoginName: "",
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "bob",
        password: availLogins.emptyXYZ.password,
      },
    },
    promptTextboxValues: {},
    expectedButtonLabel: "Update",
    // We can't end up with duplicates (bob:xyz and bob:ABC) so the following seems reasonable.
    // We could delete the emptyXYZ but we would want to intelligently merge metadata.
    resultDescription:
      "Multiple login matches but user indicated they want bob:xyz in the prompt so modify bob to give that",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "",
        password: "xyz",
      },
      {
        username: "bob",
        password: "xyz",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.emptyXYZ.guid, "Check guid");
      is(
        finalLogins[0].timeLastUsed,
        savedLoginsByName.emptyXYZ.timeLastUsed,
        "Check timeLastUsed didn't change"
      );
      is(
        finalLogins[0].timePasswordChanged,
        savedLoginsByName.emptyXYZ.timePasswordChanged,
        "Check timePasswordChanged didn't change"
      );

      is(finalLogins[1].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[1].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed did change"
      );
      ok(
        finalLogins[1].timePasswordChanged >
          savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged did change"
      );
    },
  },
  {
    name:
      "Username & password changes to an auto-saved login apply to matching usernamed-login",
    // when we update an auto-saved login - changing both username & password, is
    // the matching login updated and empty-username login removed?
    initialSavedLogins: [availLogins.emptyXYZ, availLogins.bobABC],
    autoSavedLoginName: "emptyXYZ",
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "bob",
        password: "xyz",
      },
    },
    promptTextboxValues: {
      // type a new password in the doorhanger
      password: "newpassword",
    },
    expectedButtonLabel: "Update",
    resultDescription:
      "The empty-username login is removed, other login gets the new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "bob",
        password: "newpassword",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[0].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed did change"
      );
    },
  },
  {
    name:
      "Username & password changes to a non-auto-saved login matching usernamed-login",
    // when we update a non-auto-saved login - changing both username & password, is
    // the matching login updated and empty-username login unchanged?
    initialSavedLogins: [availLogins.emptyXYZ, availLogins.bobABC],
    autoSavedLoginName: "", // no auto-saved logins for this session
    promptArgs: {
      oldLogin: "emptyXYZ",
      changeLogin: {
        username: "bob",
        password: "xyz",
      },
    },
    promptTextboxValues: {
      // type a new password in the doorhanger
      password: "newpassword",
    },
    expectedButtonLabel: "Update",
    resultDescription:
      "The empty-username login is not changed, other login gets the new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "",
        password: "xyz",
      },
      {
        username: "bob",
        password: "newpassword",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.emptyXYZ.guid, "Check guid");
      is(
        finalLogins[0].timeLastUsed,
        savedLoginsByName.emptyXYZ.timeLastUsed,
        "Check timeLastUsed didn't change"
      );
      is(
        finalLogins[0].timePasswordChanged,
        savedLoginsByName.emptyXYZ.timePasswordChanged,
        "Check timePasswordChanged didn't change"
      );
      is(finalLogins[1].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[1].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed did change"
      );
      ok(
        finalLogins[1].timePasswordChanged >
          savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged did change"
      );
    },
  },
  {
    name: "Remove the username and change password of autosaved login",
    initialSavedLogins: [availLogins.bobABC],
    autoSavedLoginName: "bobABC",
    promptArgs: {
      oldLogin: "bobABC",
      changeLogin: {
        username: "bob",
        password: "abc!", // trigger change prompt with a password change
      },
    },
    promptTextboxValues: {
      username: "",
    },
    expectedButtonLabel: "Update",
    resultDescription:
      "The auto-saved login is updated with new empty-username login and new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "",
        password: "abc!",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[0].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed did change"
      );
      ok(
        finalLogins[0].timePasswordChanged >
          savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged did change"
      );
    },
  },
  {
    name: "Remove the username and change password of non-autosaved login",
    initialSavedLogins: [availLogins.bobABC],
    // no autosaved guid
    promptArgs: {
      oldLogin: "bobABC",
      changeLogin: {
        username: "bob",
        password: "abc!", // trigger change prompt with a password change
      },
    },
    promptTextboxValues: {
      username: "",
    },
    expectedButtonLabel: "Save",
    resultDescription:
      "A new empty-username login is created with the new password",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "bob",
        password: "abc",
      },
      {
        username: "",
        password: "abc!",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      is(
        finalLogins[0].timeLastUsed,
        savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed didn't change"
      );
      is(
        finalLogins[0].timePasswordChanged,
        savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged didn't change"
      );
    },
  },
  {
    name: "Remove username from the auto-saved sole login",
    initialSavedLogins: [availLogins.bobABC],
    autoSavedLoginName: "bobABC",
    promptArgs: {
      oldLogin: "bobABC",
      changeLogin: {
        username: "bob",
        password: "abc!", // trigger change prompt with a password change
      },
    },
    promptTextboxValues: {
      username: "",
      password: "abc", // put password back to what it was
    },
    expectedButtonLabel: "Update",
    resultDescription: "The existing login is updated",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "",
        password: "abc",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      ok(
        finalLogins[0].timeLastUsed > savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed did change"
      );
      todo_is(
        finalLogins[0].timePasswordChanged,
        savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged didn't change"
      );
    },
  },
  {
    name: "Remove username from the non-auto-saved sole login",
    initialSavedLogins: [availLogins.bobABC],
    // no autoSavedLoginGuid
    promptArgs: {
      oldLogin: "bobABC",
      changeLogin: {
        username: "bob",
        password: "abc!", // trigger change prompt with a password change
      },
    },
    promptTextboxValues: {
      username: "",
      password: "abc", // put password back to what it was
    },
    expectedButtonLabel: "Save",
    resultDescription: "A new empty-username login is created",
    expectedStorageChange: true,
    expectedResultLogins: [
      {
        username: "bob",
        password: "abc",
      },
      {
        username: "",
        password: "abc",
      },
    ],
    resultCheck() {
      is(finalLogins[0].guid, savedLoginsByName.bobABC.guid, "Check guid");
      is(
        finalLogins[0].timeLastUsed,
        savedLoginsByName.bobABC.timeLastUsed,
        "Check timeLastUsed didn't change"
      );
      is(
        finalLogins[0].timePasswordChanged,
        savedLoginsByName.bobABC.timePasswordChanged,
        "Check timePasswordChanged didn't change"
      );
    },
  },
];

for (let testData of tests) {
  let tmp = {
    async [testData.name]() {
      await promptToChangePasswordTest(testData);
    },
  };
  add_task(tmp[testData.name]);
}
