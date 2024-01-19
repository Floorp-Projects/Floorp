/* eslint no-shadow:"off" */

const passwordInputSelector = "#form-basic-password";
const usernameInputSelector = "#form-basic-username";
const FORM_URL =
  "https://example.com/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons.visibilityToggle", true]],
  });
});

let testCases = [
  {
    /* Test that the doorhanger password field shows plain or * text
     * when the checkbox is checked.
     */
    name: "test_toggle_password",
    logins: [],
    enabledPrimaryPassword: false,
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "pw",
      [usernameInputSelector]: "username",
    },
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      passwordChangedDoorhanger: null,
      submitDoorhanger: {
        type: "password-save",
        dismissed: false,
        username: "username",
        password: "pw",
        toggleVisible: true,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
        afterToggleClick0: {
          inputType: "text",
          toggleChecked: true,
        },
        afterToggleClick1: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
  {
    /* Test that the doorhanger password toggle checkbox is disabled
     * when the primary password is set.
     */
    name: "test_checkbox_disabled_if_has_primary_password",
    logins: [],
    enabledPrimaryPassword: true,
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "pass",
      [usernameInputSelector]: "username",
    },
    expected: {
      initialForm: {
        username: "",
        password: "",
      },
      passwordChangedDoorhanger: null,
      submitDoorhanger: {
        type: "password-save",
        dismissed: false,
        username: "username",
        password: "pass",
        toggleVisible: false,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
  {
    /* Test that the reveal password checkbox is hidden when editing the
     * password of an autofilled login
     */
    name: "test_edit_autofilled_password",
    logins: [{ username: "username1", password: "password" }],
    formDefaults: {},
    formChanges: {
      [passwordInputSelector]: "password!",
    },
    expected: {
      initialForm: {
        username: "username1",
        password: "password",
      },
      passwordChangedDoorhanger: {
        type: "password-change",
        dismissed: true,
        username: "username1",
        password: "password!",
        toggleVisible: false,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
      submitDoorhanger: {
        type: "password-change",
        dismissed: false,
        username: "username1",
        password: "password!",
        toggleVisible: false,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
  {
    /* Test that the reveal password checkbox is shown when editing the
     * password of a login that has been autofilled and then deleted
     */
    name: "test_autofilled_cleared_then_updated_password",
    logins: [{ username: "username1", password: "password" }],
    formDefaults: {},
    formChanges: [
      {
        [passwordInputSelector]: "",
      },
      {
        [passwordInputSelector]: "password!",
      },
    ],
    expected: {
      initialForm: {
        username: "username1",
        password: "password",
      },
      passwordChangedDoorhanger: {
        type: "password-change",
        dismissed: true,
        username: "username1",
        password: "password!",
        toggleVisible: true,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
      submitDoorhanger: {
        type: "password-change",
        dismissed: false,
        username: "username1",
        password: "password!",
        toggleVisible: true,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
  {
    /* Test that the reveal password checkbox is hidden when editing the
     * username of an autofilled login
     */
    name: "test_edit_autofilled_username",
    logins: [{ username: "username1", password: "password" }],
    formDefaults: {},
    formChanges: {
      [usernameInputSelector]: "username2",
    },
    expected: {
      initialForm: {
        username: "username1",
        password: "password",
      },
      passwordChangedDoorhanger: {
        type: "password-save",
        dismissed: true,
        username: "username2",
        password: "password",
        toggleVisible: false,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
      submitDoorhanger: {
        type: "password-save",
        dismissed: false,
        username: "username2",
        password: "password",
        toggleVisible: false,
        initialToggleState: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
];

for (let testData of testCases) {
  if (testData.skip) {
    info("Skipping test:", testData.name);
    continue;
  }
  let tmp = {
    async [testData.name]() {
      await testDoorhangerToggles(testData);
    },
  };
  add_task(tmp[testData.name]);
}

/**
 * Set initial test conditions,
 * Load and populate the form,
 * Submit it and verify doorhanger toggle behavior
 */
async function testDoorhangerToggles({
  logins = [],
  formDefaults = {},
  formChanges = {},
  expected,
  enabledPrimaryPassword,
}) {
  formChanges = Array.isArray(formChanges) ? formChanges : [formChanges];

  for (let login of logins) {
    await LoginTestUtils.addLogin(login);
  }
  if (enabledPrimaryPassword) {
    LoginTestUtils.primaryPassword.enable();
  }
  let formProcessedPromise = listenForTestNotification("FormProcessed");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: FORM_URL,
    },
    async function (browser) {
      info(`Opened tab with url: ${FORM_URL}, waiting for focus`);
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      info("Waiting for form-processed message");
      await formProcessedPromise;
      await initForm(browser, formDefaults);
      await checkForm(browser, expected.initialForm);
      info("form checked");

      // some tests check the dismissed doorhanger from editing the password
      let formChanged = expected.passwordChangedDoorhanger
        ? listenForTestNotification("PasswordEditedOrGenerated")
        : Promise.resolve();
      for (let change of formChanges) {
        await changeContentFormValues(browser, change, {
          method: "paste_text",
        });
      }

      await formChanged;

      if (expected.passwordChangedDoorhanger) {
        let expectedDoorhanger = expected.passwordChangedDoorhanger;
        info("Verifying dismissed doorhanger from password change");
        let notif = await waitForDoorhanger(browser, expectedDoorhanger.type);
        Assert.ok(notif, "got notification popup");
        Assert.equal(
          notif.dismissed,
          expectedDoorhanger.dismissed,
          "Check notification dismissed property"
        );
        let { panel } = browser.ownerGlobal.PopupNotifications;
        // we will open dismissed doorhanger to check panel contents
        Assert.equal(panel.state, "closed", "Panel is initially closed");
        let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
        info("Opening the doorhanger popup");
        // synthesize click on anchor as this also blurs the form field triggering
        // a change event
        EventUtils.synthesizeMouseAtCenter(notif.anchorElement, {});
        await promiseShown;
        await TestUtils.waitForTick();
        Assert.ok(
          panel.children.length,
          `Check the popup has at least one notification (${panel.children.length})`
        );

        // Check the password-changed-capture doorhanger contents & behaviour
        info("Verifying the doorhanger");
        await verifyDoorhangerToggles(browser, notif, expectedDoorhanger);
        await hideDoorhangerPopup(notif);
      }

      if (expected.submitDoorhanger) {
        let expectedDoorhanger = expected.submitDoorhanger;
        let { panel } = browser.ownerGlobal.PopupNotifications;
        // submit the form and wait for the doorhanger
        info("Submitting the form");
        let submittedPromise = listenForTestNotification("ShowDoorhanger");
        let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
        await submitForm(browser, "/");
        await submittedPromise;
        info("Waiting for doorhanger popup to open");
        await promiseShown;
        let notif = await getCaptureDoorhanger(expectedDoorhanger.type);
        Assert.ok(notif, "got notification popup");
        Assert.equal(
          notif.dismissed,
          expectedDoorhanger.dismissed,
          "Check notification dismissed property"
        );
        Assert.ok(
          panel.children.length,
          `Check the popup has at least one notification (${panel.children.length})`
        );

        // Check the submit-capture doorhanger contents & behaviour
        info("Verifying the submit doorhanger");
        await verifyDoorhangerToggles(browser, notif, expectedDoorhanger);
        await cleanupDoorhanger(notif);
      }
    }
  );
  await LoginTestUtils.clearData();
  if (enabledPrimaryPassword) {
    LoginTestUtils.primaryPassword.disable();
  }
  await cleanupPasswordNotifications();
}

// --------------------------------------------------------------------
// Helpers

async function verifyDoorhangerToggles(browser, notif, expected) {
  let { initialToggleState, afterToggleClick0, afterToggleClick1 } = expected;

  let { panel } = browser.ownerGlobal.PopupNotifications;
  let notificationElement = panel.childNodes[0];
  let passwordTextbox = notificationElement.querySelector(
    "#password-notification-password"
  );
  let toggleCheckbox = notificationElement.querySelector(
    "#password-notification-visibilityToggle"
  );
  Assert.equal(panel.state, "open", "Panel is open");
  Assert.ok(
    BrowserTestUtils.isVisible(passwordTextbox),
    "The doorhanger password field is visible"
  );

  await checkDoorhangerUsernamePassword(expected.username, expected.password);
  if (expected.toggleVisible) {
    Assert.ok(
      BrowserTestUtils.isVisible(toggleCheckbox),
      "The visibility checkbox is shown"
    );
  } else {
    Assert.ok(
      BrowserTestUtils.isHidden(toggleCheckbox),
      "The visibility checkbox is hidden"
    );
  }

  if (initialToggleState) {
    Assert.equal(
      toggleCheckbox.checked,
      initialToggleState.toggleChecked,
      `Initially, toggle is ${
        initialToggleState.toggleChecked ? "checked" : "unchecked"
      }`
    );
    Assert.equal(
      passwordTextbox.type,
      initialToggleState.inputType,
      `Initially, password input has type: ${initialToggleState.inputType}`
    );
  }
  if (afterToggleClick0) {
    Assert.ok(
      !toggleCheckbox.hidden,
      "The checkbox shouldnt be hidden when clicking on it"
    );
    info("Clicking on the visibility toggle");
    await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
    await TestUtils.waitForTick();
    Assert.equal(
      toggleCheckbox.checked,
      afterToggleClick0.toggleChecked,
      `After 1st click, expect toggle to be checked? ${afterToggleClick0.toggleChecked}, actual: ${toggleCheckbox.checked}`
    );
    Assert.equal(
      passwordTextbox.type,
      afterToggleClick0.inputType,
      `After 1st click, expect password input to have type: ${afterToggleClick0.inputType}`
    );
  }
  if (afterToggleClick1) {
    Assert.ok(
      !toggleCheckbox.hidden,
      "The checkbox shouldnt be hidden when clicking on it"
    );
    info("Clicking on the visibility toggle again");
    await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
    await TestUtils.waitForTick();
    Assert.equal(
      toggleCheckbox.checked,
      afterToggleClick1.toggleChecked,
      `After 2nd click, expect toggle to be checked? ${afterToggleClick0.toggleChecked}, actual: ${toggleCheckbox.checked}`
    );
    Assert.equal(
      passwordTextbox.type,
      afterToggleClick1.inputType,
      `After 2nd click, expect password input to have type: ${afterToggleClick1.inputType}`
    );
  }
}

async function initForm(browser, formDefaults) {
  await ContentTask.spawn(
    browser,
    formDefaults,
    async function (selectorValues = {}) {
      for (let [sel, value] of Object.entries(selectorValues)) {
        content.document.querySelector(sel).value = value;
      }
    }
  );
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
        Assert.equal(
          field.value,
          value,
          sel + " has the expected initial value"
        );
      }
    }
  );
}

async function submitForm(browser, action = "") {
  // Submit the form
  let correctPathNamePromise = BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(browser, [action], async function (actionPathname) {
    let form = content.document.querySelector("form");
    if (actionPathname) {
      form.action = actionPathname;
    }
    info("Submitting form to:" + form.action);
    form.submit();
    info("Submitted the form");
  });
  await correctPathNamePromise;
  await SpecialPowers.spawn(browser, [action], async actionPathname => {
    let win = content;
    await ContentTaskUtils.waitForCondition(() => {
      return (
        win.location.pathname == actionPathname &&
        win.document.readyState == "complete"
      );
    }, "Wait for form submission load");
  });
}
