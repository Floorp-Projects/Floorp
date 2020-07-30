/**
 * Test changed (not submitted) passwords produce the right doorhangers/notifications
 */

/* eslint no-shadow:"off" */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";
const passwordInputSelector = "#form-basic-password";
const usernameInputSelector = "#form-basic-username";

let testCases = [
  {
    name: "Enter password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "abcXYZ",
    },
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "",
        password: "abcXYZ",
        toggle: "visible",
      },
    },
  },
  {
    name: "Change password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [],
    formDefaults: {
      [passwordInputSelector]: "pass1",
    },
    formChanges: {
      [passwordInputSelector]: "pass-changed",
    },
    expected: {
      initialForm: {
        username: "",
        password: "pass1",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "",
        password: "pass-changed",
        toggle: "visible",
      },
    },
  },
  {
    name: "Change autofilled password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "user1", password: "autopass" }],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "autopass-changed",
    },
    expected: {
      initialForm: {
        username: "user1",
        password: "autopass",
      },
      doorhanger: {
        type: "password-change",
        dismissed: true,
        anchorExtraAttr: "",
        username: "user1",
        password: "autopass-changed",
      },
    },
  },
  {
    name: "Change autofilled username and password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "user1", password: "pass1" }],
    formDefaults: {},
    formChanges: {
      [usernameInputSelector]: "user2",
      [passwordInputSelector]: "pass2",
    },
    expected: {
      initialForm: {
        username: "user1",
        password: "pass1",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "user2",
        password: "pass2",
        toggle: "visible",
      },
    },
  },
  {
    name: "Change password pref disabled",
    prefEnabled: false,
    isLoggedIn: true,
    logins: [],
    formDefaults: {
      [passwordInputSelector]: "pass1",
    },
    formChanges: {
      [passwordInputSelector]: "pass-changed",
    },
    expected: {
      initialForm: {
        username: "",
        password: "pass1",
      },
      doorhanger: null,
    },
  },
  {
    name: "Change to new username",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "user1", password: "pass1" }],
    formDefaults: {},
    formChanges: {
      [usernameInputSelector]: "user2",
    },
    expected: {
      initialForm: {
        username: "user1",
        password: "pass1",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "user2",
        password: "pass1",
        toggle: "visible",
      },
    },
  },
  {
    name: "Change to existing username, different password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "user-saved", password: "pass1" }],
    formDefaults: {
      [usernameInputSelector]: "user-prefilled",
      [passwordInputSelector]: "pass2",
    },
    formChanges: {
      [usernameInputSelector]: "user-saved",
    },
    expected: {
      initialForm: {
        username: "user-prefilled",
        password: "pass2",
      },
      doorhanger: {
        type: "password-change",
        dismissed: true,
        anchorExtraAttr: "",
        username: "user-saved",
        password: "pass2",
        toggle: "visible",
      },
    },
  },
  {
    name: "Add username to existing password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "", password: "pass1" }],
    formDefaults: {},
    formChanges: {
      [usernameInputSelector]: "user1",
    },
    expected: {
      initialForm: {
        username: "",
        password: "pass1",
      },
      doorhanger: {
        type: "password-change",
        dismissed: true,
        anchorExtraAttr: "",
        username: "user1",
        password: "pass1",
        toggle: "visible",
      },
    },
  },
  {
    name: "Change to existing username, password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "user1", password: "pass1" }],
    formDefaults: {
      [usernameInputSelector]: "user",
      [passwordInputSelector]: "pass",
    },
    formChanges: {
      [passwordInputSelector]: "pass1",
      [usernameInputSelector]: "user1",
    },
    expected: {
      initialForm: {
        username: "user",
        password: "pass",
      },
      doorhanger: null,
    },
  },
  {
    name:
      "Ensure a dismissed password-save doorhanger appears on an input event when editing an unsaved password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "a",
    },
    shouldBlur: false,
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "",
        password: "a",
        toggle: "visible",
      },
    },
  },
  {
    name:
      "Ensure a dismissed password-save doorhanger appears with the latest input value upon editing an unsaved password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "a",
      [passwordInputSelector]: "ab",
      [passwordInputSelector]: "abc",
    },
    shouldBlur: false,
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      doorhanger: {
        type: "password-save",
        dismissed: true,
        anchorExtraAttr: "",
        username: "",
        password: "abc",
        toggle: "visible",
      },
    },
  },
  {
    name:
      "Ensure a dismissed password-change doorhanger appears on an input event when editing a saved password",
    prefEnabled: true,
    isLoggedIn: true,
    logins: [{ username: "", password: "pass1" }],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "pass",
    },
    shouldBlur: false,
    expected: {
      initialForm: {
        username: "",
        password: "pass1",
      },
      doorhanger: {
        type: "password-change",
        dismissed: true,
        anchorExtraAttr: "",
        username: "",
        password: "pass",
        toggle: "visible",
      },
    },
  },
  {
    name:
      "Ensure no dismissed doorhanger is shown on 'input' when Primary Password is locked",
    prefEnabled: true,
    isLoggedIn: false,
    logins: [],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "pass",
    },
    shouldBlur: false,
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      doorhanger: null,
    },
  },
  {
    name:
      "Ensure no dismissed doorhanger is shown on 'change' when Primary Password is locked",
    prefEnabled: true,
    isLoggedIn: false,
    logins: [],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "pass",
    },
    shouldBlur: true,
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      doorhanger: null,
    },
  },
];

requestLongerTimeout(2);
SimpleTest.requestCompleteLog();

for (let testData of testCases) {
  let tmp = {
    async [testData.name]() {
      await SpecialPowers.pushPrefEnv({
        set: [["signon.passwordEditCapture.enabled", testData.prefEnabled]],
      });
      if (!testData.isLoggedIn) {
        // Enable Primary Password
        LoginTestUtils.masterPassword.enable();
      }
      for (let passwordFieldType of ["password", "text"]) {
        info(
          "testing with type=" +
            passwordFieldType +
            ": " +
            JSON.stringify(testData)
        );
        await testPasswordChange(testData, { passwordFieldType });
      }
      if (!testData.isLoggedIn) {
        LoginTestUtils.masterPassword.disable();
      }
      await SpecialPowers.popPrefEnv();
    },
  };
  add_task(tmp[testData.name]);
}

async function waitForPromise(promise, timeoutMs = 5000) {
  let timedOut = new Promise((resolve, reject) => {
    /* eslint-disable-next-line mozilla/no-arbitrary-setTimeout */
    let timerId = setTimeout(() => {
      clearTimeout(timerId);
      reject(`Timed out in ${timeoutMs} ms.`);
    }, timeoutMs);
  });
  await Promise.race([promise, timedOut]);
}

async function testPasswordChange(
  {
    logins = [],
    formDefaults = {},
    formChanges = {},
    expected,
    isLoggedIn,
    shouldBlur = true,
  },
  { passwordFieldType }
) {
  await LoginTestUtils.clearData();
  await cleanupDoorhanger();

  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  for (let login of logins) {
    await LoginTestUtils.addLogin(login);
  }

  for (let login of Services.logins.getAllLogins()) {
    info(`Saved login: ${login.username}, ${login.password}, ${login.origin}`);
  }

  let formProcessedPromise = listenForTestNotification("FormProcessed");
  info("Opening tab with url: " + url);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      info(`Opened tab with url: ${url}, waiting for focus`);
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      info("Waiting for form-processed message");
      await formProcessedPromise;
      await initForm(browser, formDefaults, { passwordFieldType });
      await checkForm(browser, expected.initialForm);
      info("form checked");

      let passwordEditedMessage = listenForTestNotification(
        "PasswordEditedOrGenerated"
      );

      await changeContentFormValues(browser, formChanges, shouldBlur);
      info(
        "form edited, waiting for test notification of PasswordEditedOrGenerated"
      );

      try {
        await waitForPromise(passwordEditedMessage, 5000);
        // A message is still sent to the parent process when Primary Password is enabled
        ok(expected.doorhanger || !isLoggedIn, "Message sent");
      } catch (ex) {
        ok(!expected.doorhanger, "No message sent");
      }
      info("Resolved listenForTestNotification promise");

      if (!expected.doorhanger) {
        let notif;
        try {
          await TestUtils.waitForCondition(
            () => {
              return (notif = PopupNotifications.getNotification(
                "password",
                browser
              ));
            },
            `Waiting to ensure no notification`,
            undefined,
            25
          );
        } catch (ex) {}
        ok(!notif, "No doorhanger expected");
        // the remainder of the test is for doorhanger-expected cases
        return;
      }

      let notificationType = expected.doorhanger.type;
      ok(
        /^password-save|password-change$/.test(notificationType),
        "test provided an expected notification type: " + notificationType
      );
      info("waiting for doorhanger");
      await waitForDoorhanger(browser, notificationType);

      info("verifying doorhanger");
      let notif = await openAndVerifyDoorhanger(
        browser,
        notificationType,
        expected.doorhanger
      );
      ok(notif, "Doorhanger was shown");

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
      await promiseHidden;

      info("cleanup doorhanger");
      await cleanupDoorhanger(notif);
    }
  );
}

async function initForm(browser, formDefaults, passwordFieldType) {
  await ContentTask.spawn(
    browser,
    { passwordInputSelector, passwordFieldType },
    async function({ passwordInputSelector, passwordFieldType }) {
      content.document.querySelector(
        passwordInputSelector
      ).type = passwordFieldType;
    }
  );
  await ContentTask.spawn(browser, formDefaults, async function(
    selectorValues
  ) {
    for (let [sel, value] of Object.entries(selectorValues)) {
      content.document.querySelector(sel).value = value;
    }
  });
}

async function checkForm(browser, expected) {
  await ContentTask.spawn(
    browser,
    {
      [passwordInputSelector]: expected.password,
      [usernameInputSelector]: expected.username,
    },
    async function contentCheckForm(selectorValues) {
      for (let [sel, value] of Object.entries(selectorValues)) {
        let field = content.document.querySelector(sel);
        is(field.value, value, sel + " has the expected initial value");
      }
    }
  );
}

async function openAndVerifyDoorhanger(browser, type, expected) {
  // check a dismissed prompt was shown with extraAttr attribute
  let notif = getCaptureDoorhanger(type);
  ok(notif, `${type} doorhanger was created`);
  is(
    notif.dismissed,
    expected.dismissed,
    "Check notification dismissed property"
  );
  is(
    notif.anchorElement.getAttribute("extraAttr"),
    expected.anchorExtraAttr,
    "Check icon extraAttr attribute"
  );
  let { panel } = PopupNotifications;
  // if the doorhanged is dimissed, we will open it to check panel contents
  is(panel.state, "closed", "Panel is initially closed");
  let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
  // synthesize click on anchor as this also blurs the form field triggering
  // a change event
  EventUtils.synthesizeMouseAtCenter(notif.anchorElement, {});
  await promiseShown;
  await Promise.resolve();
  await checkDoorhangerUsernamePassword(expected.username, expected.password);

  let notificationElement = PopupNotifications.panel.childNodes[0];
  let toggleCheckboxHidden = notificationElement
    .querySelector("#password-notification-visibilityToggle")
    .getAttribute("hidden");

  if (expected.toggle == "visible") {
    todo(!toggleCheckboxHidden, "Toggle checkbox visible as expected");
  } else if (expected.toggle == "hidden") {
    todo(toggleCheckboxHidden, "Toggle checkbox hidden as expected");
  } else {
    info("Not checking toggle checkbox visibility");
  }
  return notif;
}
