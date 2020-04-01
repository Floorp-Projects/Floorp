const passwordInputSelector = "#form-basic-password";
const usernameInputSelector = "#form-basic-username";
const FORM_URL =
  "https://example.com/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";

add_task(async function setup() {
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
    enabledMasterPassword: false,
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
     * when the master password is set.
     */
    name: "test_checkbox_disabled_if_has_master_password",
    logins: [],
    enabledMasterPassword: true,
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
        // ensure nothing changes if we attempt to click on it
        afterToggleClick0: {
          inputType: "password",
          toggleChecked: false,
        },
      },
    },
  },
];

for (let testData of testCases) {
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
  enabledMasterPassword,
}) {
  for (let login of logins) {
    await LoginTestUtils.addLogin(login);
  }
  if (enabledMasterPassword) {
    LoginTestUtils.masterPassword.enable();
  }
  let formProcessedPromise = listenForTestNotification("FormProcessed");
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: FORM_URL,
    },
    async function(browser) {
      info(`Opened tab with url: ${FORM_URL}, waiting for focus`);
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      info("Waiting for form-processed message");
      await formProcessedPromise;
      await initForm(browser, formDefaults);
      await checkForm(browser, expected.initialForm);
      info("form checked");

      // some tests check the dismissed doorhanger from editing the password
      if (passwordInputSelector in formChanges) {
        let passwordEditedMessage = listenForTestNotification(
          "PasswordEditedOrGenerated"
        );
        await changeContentFormValues(browser, formChanges);
        info(
          "form edited, waiting for test notification of PasswordEditedOrGenerated"
        );
        await passwordEditedMessage;
      }

      // submit the form and wait for the doorhanger
      let submittedPromise = listenForTestNotification("FormSubmit");
      SpecialPowers.spawn(browser, [], () => {
        content.document.getElementById("form-basic").submit();
      });
      await submittedPromise;
      let notif = await getCaptureDoorhangerThatMayOpen(
        expected.submitDoorhanger.type
      );
      ok(notif, "got notification popup");

      // Check the actual content of the popup notification.
      await checkDoorhangerUsernamePassword(
        expected.submitDoorhanger.username,
        expected.submitDoorhanger.password
      );

      let notificationElement = PopupNotifications.panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector(
        "#password-notification-password"
      );
      let toggleCheckbox = notificationElement.querySelector(
        "#password-notification-visibilityToggle"
      );
      let {
        toggleVisible,
        initialToggleState,
        afterToggleClick0,
        afterToggleClick1,
      } = expected.submitDoorhanger;

      if (toggleVisible) {
        ok(
          BrowserTestUtils.is_visible(toggleCheckbox),
          "The visibility checkbox is shown"
        );
      } else {
        ok(
          BrowserTestUtils.is_hidden(toggleCheckbox),
          "The visibility checkbox is hidden"
        );
      }

      if (initialToggleState) {
        is(
          toggleCheckbox.checked,
          initialToggleState.toggleChecked,
          `Initially, toggle is ${
            initialToggleState.toggleChecked ? "checked" : "unchecked"
          }`
        );
        is(
          passwordTextbox.type,
          initialToggleState.inputType,
          `Initially, password input has type: ${initialToggleState.inputType}`
        );
      }
      if (afterToggleClick0) {
        info("Clicking on the visibility toggle");
        await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
        await TestUtils.waitForTick();
        is(
          toggleCheckbox.checked,
          afterToggleClick0.toggleChecked,
          `After 1st click, expect toggle to be checked? ${afterToggleClick0.toggleChecked}, actual: ${toggleCheckbox.checked}`
        );
        is(
          passwordTextbox.type,
          afterToggleClick0.inputType,
          `After 1st click, expect password input to have type: ${afterToggleClick0.inputType}`
        );
      }
      if (afterToggleClick1) {
        info("Clicking on the visibility toggle again");
        await EventUtils.synthesizeMouseAtCenter(toggleCheckbox, {});
        await TestUtils.waitForTick();
        is(
          toggleCheckbox.checked,
          afterToggleClick1.toggleChecked,
          `After 2nd click, expect toggle to be checked? ${afterToggleClick0.toggleChecked}, actual: ${toggleCheckbox.checked}`
        );
        is(
          passwordTextbox.type,
          afterToggleClick1.inputType,
          `After 2nd click, expect password input to have type: ${afterToggleClick1.inputType}`
        );
      }
      await cleanupDoorhanger(notif);
    }
  );
  await LoginTestUtils.clearData();
  if (enabledMasterPassword) {
    LoginTestUtils.masterPassword.disable();
  }
}

// --------------------------------------------------------------------
// Helpers

async function initForm(browser, formDefaults) {
  await ContentTask.spawn(browser, formDefaults, async function(
    selectorValues = {}
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
