/**
 * Test using the generated passwords produces the right doorhangers/notifications
 */

/* eslint no-shadow:"off" */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const FORM_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const passwordInputSelector = "#form-basic-password";
const usernameInputSelector = "#form-basic-username";

requestLongerTimeout(2);

async function task_setup() {
  Services.logins.removeAllUserFacingLogins();
  LoginTestUtils.resetGeneratedPasswordsCache();
  await cleanupPasswordNotifications();
  await LoginTestUtils.remoteSettings.setupImprovedPasswordRules();
}

async function setup_withOneLogin(username = "username", password = "pass1") {
  // Reset to a single, known login
  await task_setup();
  let login = await LoginTestUtils.addLogin({ username, password });
  return login;
}

async function setup_withNoLogins() {
  // Reset to a single, known login
  await task_setup();
  Assert.equal(
    (await Services.logins.getAllLogins()).length,
    0,
    "0 logins at the start of the test"
  );
}

async function fillGeneratedPasswordFromACPopup(
  browser,
  passwordInputSelector
) {
  let popup = document.getElementById("PopupAutoComplete");
  Assert.ok(popup, "Got popup");
  await openACPopup(popup, browser, passwordInputSelector);
  await fillGeneratedPasswordFromOpenACPopup(browser, passwordInputSelector);
}

async function checkPromptContents(
  anchorElement,
  browser,
  expectedPasswordLength = 0
) {
  let { panel } = PopupNotifications;
  Assert.ok(PopupNotifications.isPanelOpen, "Confirm popup is open");
  let notificationElement = panel.childNodes[0];
  if (expectedPasswordLength) {
    info(
      `Waiting for password value to be ${expectedPasswordLength} chars long`
    );
    await BrowserTestUtils.waitForCondition(() => {
      return (
        notificationElement.querySelector("#password-notification-password")
          .value.length == expectedPasswordLength
      );
    }, "Wait for nsLoginManagerPrompter writeDataToUI()");
  }

  return {
    passwordValue: notificationElement.querySelector(
      "#password-notification-password"
    ).value,
    usernameValue: notificationElement.querySelector(
      "#password-notification-username"
    ).value,
  };
}

async function verifyGeneratedPasswordWasFilled(
  browser,
  passwordInputSelector
) {
  await SpecialPowers.spawn(
    browser,
    [[passwordInputSelector]],
    function checkFinalFieldValue(inputSelector) {
      let { LoginTestUtils: LTU } = ChromeUtils.importESModule(
        "resource://testing-common/LoginTestUtils.sys.mjs"
      );
      let passwordInput = content.document.querySelector(inputSelector);
      Assert.equal(
        passwordInput.value.length,
        LTU.generation.LENGTH,
        "Password field was filled with generated password"
      );
    }
  );
}

async function openFormInNewTab(url, formValues, taskFn) {
  let formFilled = listenForTestNotification("FormProcessed");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await formFilled;

      await SpecialPowers.spawn(
        browser,
        [formValues],
        async function prepareAndCheckForm({
          password: passwordProps,
          username: usernameProps,
        }) {
          let doc = content.document;
          // give the form an action so we can know when submit is complete
          doc.querySelector("form").action = "/";

          let props = passwordProps;
          if (props) {
            // We'll reuse the form_basic.html, but ensure we'll get the generated password autocomplete option
            let field = doc.querySelector(props.selector);
            if (props.type) {
              // Change the type from 'password' to something else.
              field.type = props.type;
            }

            field.setAttribute("autocomplete", "new-password");
            if (props.hasOwnProperty("expectedValue")) {
              Assert.equal(
                field.value,
                props.expectedValue,
                "Check autofilled password value"
              );
            }
          }
          props = usernameProps;
          if (props) {
            let field = doc.querySelector(props.selector);
            if (props.hasOwnProperty("expectedValue")) {
              Assert.equal(
                field.value,
                props.expectedValue,
                "Check autofilled username value"
              );
            }
          }
        }
      );

      if (formValues.password && formValues.password.setValue !== undefined) {
        info(
          "Editing the password, expectedMessage? " +
            formValues.password.expectedMessage
        );
        let messagePromise = formValues.password.expectedMessage
          ? listenForTestNotification(formValues.password.expectedMessage)
          : Promise.resolve();
        await changeContentInputValue(
          browser,
          formValues.password.selector,
          formValues.password.setValue
        );
        await messagePromise;
        info("messagePromise resolved");
      }

      if (formValues.username && formValues.username.setValue !== undefined) {
        info(
          "Editing the username, expectedMessage? " +
            formValues.username.expectedMessage
        );
        let messagePromise = formValues.username.expectedMessage
          ? listenForTestNotification(formValues.username.expectedMessage)
          : Promise.resolve();
        await changeContentInputValue(
          browser,
          formValues.username.selector,
          formValues.username.setValue
        );
        await messagePromise;
        info("messagePromise resolved");
      }

      await taskFn(browser);
      await closePopup(
        browser.ownerDocument.getElementById("confirmation-hint")
      );
    }
  );
}

async function openAndVerifyDoorhanger(browser, type, expected) {
  // check a dismissed prompt was shown with extraAttr attribute
  let notif = getCaptureDoorhanger(type);
  Assert.ok(notif, `${type} doorhanger was created`);
  Assert.equal(
    notif.dismissed,
    expected.dismissed,
    "Check notification dismissed property"
  );
  Assert.equal(
    notif.anchorElement.getAttribute("extraAttr"),
    expected.anchorExtraAttr,
    "Check icon extraAttr attribute"
  );
  let { panel } = PopupNotifications;
  // if the doorhanged is dimissed, we will open it to check panel contents
  if (panel.state !== "open") {
    let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
    if (panel.state !== "showing") {
      // synthesize click on anchor as this also blurs the form field triggering
      // a change event
      EventUtils.synthesizeMouseAtCenter(notif.anchorElement, {});
    }
    await promiseShown;
  }
  let { passwordValue, usernameValue } = await checkPromptContents(
    notif.anchorElement,
    browser,
    expected.passwordLength
  );
  Assert.equal(
    passwordValue.length,
    expected.passwordLength || LoginTestUtils.generation.LENGTH,
    "Doorhanger password field has generated 15-char value"
  );
  Assert.equal(
    usernameValue,
    expected.usernameValue,
    "Doorhanger username field was popuplated"
  );
  return notif;
}

async function appendContentInputvalue(browser, selector, str) {
  await ContentTask.spawn(
    browser,
    { selector, str },
    async function ({ selector, str }) {
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      let input = content.document.querySelector(selector);
      input.focus();
      input.select();
      await EventUtils.synthesizeKey("KEY_ArrowRight", {}, content);
      let changedPromise = ContentTaskUtils.waitForEvent(input, "change");
      if (str) {
        await EventUtils.sendString(str, content);
      }
      input.blur();
      await changedPromise;
    }
  );
  info("Input value changed");
  await TestUtils.waitForTick();
}

async function submitForm(browser) {
  // Submit the form
  info("Now submit the form");
  let correctPathNamePromise = BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(browser, [], async function () {
    content.document.querySelector("form").submit();
  });
  await correctPathNamePromise;
  await SpecialPowers.spawn(browser, [], async () => {
    let win = content;
    await ContentTaskUtils.waitForCondition(() => {
      return (
        win.location.pathname == "/" && win.document.readyState == "complete"
      );
    }, "Wait for form submission load");
  });
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.generation.available", true],
      ["signon.generation.enabled", true],
    ],
  });
  // assert that there are no logins
  let logins = await Services.logins.getAllLogins();
  Assert.equal(logins.length, 0, "There are no logins");
});

add_task(async function autocomplete_generated_password_auto_saved() {
  // confirm behavior when filling a generated password via autocomplete
  // when there are no other logins
  await setup_withNoLogins();
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: { selector: passwordInputSelector, expectedValue: "" },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      // Let the hint hide itself this first time
      let forceClosePopup = false;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      let [{ username, password }] = await storageChangedPromise;
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      // Check properties of the newly auto-saved login
      Assert.equal(username, "", "Saved login should have no username");
      Assert.equal(
        password.length,
        LoginTestUtils.generation.LENGTH,
        "Saved login should have generated password"
      );

      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
      await promiseHidden;

      // confirm the extraAttr attribute is removed after opening & dismissing the doorhanger
      Assert.ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if the extraAttr attribute was removed"
      );
      await cleanupDoorhanger(notif);

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      let [autoSavedLogin] = await Services.logins.getAllLogins();
      info("waiting for submitForm");
      await submitForm(browser);
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: autoSavedLogin.timesUsed + 1,
          username: "",
        },
      ]);
    }
  );
});

add_task(
  async function autocomplete_generated_password_with_confirm_field_auto_saved() {
    // confirm behavior when filling a generated password via autocomplete
    // when there are no other logins and the form has a confirm password field
    const FORM_WITH_CONFIRM_FIELD_PAGE_PATH =
      "/browser/toolkit/components/passwordmgr/test/browser/form_basic_with_confirm_field.html";
    const confirmPasswordInputSelector = "#form-basic-confirm-password";
    await setup_withNoLogins();
    await openFormInNewTab(
      TEST_ORIGIN + FORM_WITH_CONFIRM_FIELD_PAGE_PATH,
      {
        password: { selector: passwordInputSelector, expectedValue: "" },
        username: { selector: usernameInputSelector, expectedValue: "" },
      },
      async function taskFn(browser) {
        let storageChangedPromise = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (_, data) => data == "addLogin"
        );
        // Let the hint hide itself this first time
        let forceClosePopup = false;
        let hintShownAndVerified = verifyConfirmationHint(
          browser,
          forceClosePopup
        );

        await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
        let [{ username, password }] = await storageChangedPromise;
        await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);
        await verifyGeneratedPasswordWasFilled(
          browser,
          confirmPasswordInputSelector
        );

        // Make sure confirmation hint was shown
        info("waiting for verifyConfirmationHint");
        await hintShownAndVerified;

        // Check properties of the newly auto-saved login
        Assert.equal(username, "", "Saved login should have no username");
        Assert.equal(
          password.length,
          LoginTestUtils.generation.LENGTH,
          "Saved login should have generated password"
        );

        let notif = await openAndVerifyDoorhanger(browser, "password-change", {
          dismissed: true,
          anchorExtraAttr: "attention",
          usernameValue: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        });

        let promiseHidden = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popuphidden"
        );
        clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
        await promiseHidden;

        // confirm the extraAttr attribute is removed after opening & dismissing the doorhanger
        Assert.ok(
          !notif.anchorElement.hasAttribute("extraAttr"),
          "Check if the extraAttr attribute was removed"
        );
        await cleanupDoorhanger(notif);

        storageChangedPromise = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (_, data) => data == "modifyLogin"
        );
        let [autoSavedLogin] = await Services.logins.getAllLogins();
        info("waiting for submitForm");
        await submitForm(browser);
        await storageChangedPromise;
        await verifyLogins([
          {
            timesUsed: autoSavedLogin.timesUsed + 1,
            username: "",
          },
        ]);
      }
    );
  }
);

add_task(async function autocomplete_generated_password_saved_empty_username() {
  // confirm behavior when filling a generated password via autocomplete
  // when there is an existing saved login with a "" username
  await setup_withOneLogin("", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = await Services.logins.getAllLogins();
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      await waitForDoorhanger(browser, "password-change");
      info("Waiting to openAndVerifyDoorhanger");
      await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });
      await hideDoorhangerPopup();
      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      info("waiting for submitForm");
      await submitForm(browser);
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );
});

add_task(async function autocomplete_generated_password_saved_username() {
  // confirm behavior when filling a generated password via autocomplete
  // into a form with username matching an existing saved login
  await setup_withOneLogin("user1", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "user1",
      },
    },
    async function taskFn(browser) {
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      // We don't need to wait to confirm the hint hides itelf every time
      let forceClosePopup = true;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      info("waiting for addLogin");
      await storageChangedPromise;
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      // Check properties of the newly auto-saved login
      let [user1LoginSnapshot, autoSavedLogin] = await verifyLogins([
        {
          username: "user1",
          password: "xyzpassword", // user1 is unchanged
        },
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);

      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "user1",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
      await promiseHidden;

      // confirm the extraAttr attribute is removed after opening & dismissing the doorhanger
      Assert.ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if the extraAttr attribute was removed"
      );
      await cleanupDoorhanger(notif);

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      info("waiting for submitForm");
      await submitForm(browser);
      promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: user1LoginSnapshot.timesUsed + 1,
          username: "user1",
          password: autoSavedLogin.password,
        },
      ]);
    }
  );
});

add_task(async function ac_gen_pw_saved_empty_un_stored_non_empty_un_in_form() {
  // confirm behavior when when the form's username field has a non-empty value
  // and there is an existing saved login with a "" username
  await setup_withOneLogin("", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "",
        setValue: "myusername",
        // with an empty password value, no message is sent for a username change
        expectedMessage: "",
      },
    },
    async function taskFn(browser) {
      let [savedLogin] = await Services.logins.getAllLogins();
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      await waitForDoorhanger(browser, "password-save");
      info("Waiting to openAndVerifyDoorhanger");
      await openAndVerifyDoorhanger(browser, "password-save", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: "myusername",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });
      await hideDoorhangerPopup();
      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      info("waiting for submitForm");
      await submitForm(browser);
      let notif = await openAndVerifyDoorhanger(browser, "password-save", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "myusername",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, REMEMBER_BUTTON);
      await promiseHidden;

      info("Waiting for addLogin");
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: savedLogin.timesUsed,
          username: "",
          password: "xyzpassword",
        },
        {
          timesUsed: 1,
          username: "myusername",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );
});

add_task(async function contextfill_generated_password_saved_empty_username() {
  // confirm behavior when filling a generated password via context menu
  // when there is an existing saved login with a "" username
  await setup_withOneLogin("", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = await Services.logins.getAllLogins();
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );
      await waitForDoorhanger(browser, "password-change");
      info("Waiting to openAndVerifyDoorhanger");
      await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });
      await hideDoorhangerPopup();
      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      info("waiting for submitForm");
      await submitForm(browser);
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );
});

async function autocomplete_generated_password_edited_no_auto_save(
  passwordType = "password"
) {
  // confirm behavior when filling a generated password via autocomplete
  // when there is an existing saved login with a "" username and then editing
  // the password and autocompleting again.
  await setup_withOneLogin("", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        type: passwordType,
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = await Services.logins.getAllLogins();
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      info(
        "Filled generated password, waiting for dismissed password-change doorhanger"
      );
      await waitForDoorhanger(browser, "password-change");
      info("Waiting to openAndVerifyDoorhanger");
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      await BrowserTestUtils.sendChar("!", browser);
      await BrowserTestUtils.sendChar("@", browser);
      await BrowserTestUtils.synthesizeKey("KEY_Tab", undefined, browser);

      await waitForDoorhanger(browser, "password-change");
      info("Waiting to openAndVerifyDoorhanger");
      notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH + 2,
      });

      promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, DONT_CHANGE_BUTTON);
      await promiseHidden;

      await verifyLogins([
        {
          timesUsed: savedLogin.timesUsed,
          username: "",
          password: "xyzpassword",
        },
      ]);

      info("waiting for submitForm");
      await submitForm(browser);
      notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH + 2,
      });

      promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      await verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );

  LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().clear();
}

add_task(autocomplete_generated_password_edited_no_auto_save);

add_task(
  async function autocomplete_generated_password_edited_no_auto_save_type_text() {
    await autocomplete_generated_password_edited_no_auto_save("text");
  }
);

add_task(async function contextmenu_fill_generated_password_and_set_username() {
  // test when filling with a generated password and editing the username in the form
  // * the prompt should display the form's username
  // * the auto-saved login should have "" for username
  // * confirming the prompt should edit the "" login and add the username
  await setup_withOneLogin("olduser", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "olduser",
        setValue: "differentuser",
        // with an empty password value, no message is sent for a username change
        expectedMessage: "",
      },
    },
    async function taskFn(browser) {
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      await SpecialPowers.spawn(
        browser,
        [[passwordInputSelector, usernameInputSelector]],
        function checkEmptyPasswordField([passwordSelector, usernameSelector]) {
          Assert.equal(
            content.document.querySelector(passwordSelector).value,
            "",
            "Password field is empty"
          );
        }
      );

      // Let the hint hide itself this first time
      let forceClosePopup = false;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      info("waiting to fill generated password using context menu");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;
      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");

      info("waiting for addLogin");
      await storageChangedPromise;

      // Check properties of the newly auto-saved login
      await verifyLogins([
        null, // ignore the first one
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);

      info("Waiting to openAndVerifyDoorhanger");
      await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "differentuser",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });
      await hideDoorhangerPopup();
      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      info("waiting for submitForm");
      await submitForm(browser);
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "differentuser",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      await verifyLogins([
        null,
        {
          username: "differentuser",
          passwordLength: LoginTestUtils.generation.LENGTH,
          timesUsed: 2,
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );
});

add_task(async function contextmenu_password_change_form_without_username() {
  // test doorhanger behavior when a generated password is filled into a change-password
  // form with no username
  await setup_withOneLogin("user1", "xyzpassword");
  await LoginTestUtils.addLogin({ username: "username2", password: "pass2" });
  const passwordInputSelector = "#newpass";

  const CHANGE_FORM_PATH =
    "/browser/toolkit/components/passwordmgr/test/browser/form_password_change.html";
  await openFormInNewTab(
    TEST_ORIGIN + CHANGE_FORM_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "",
      },
    },
    async function taskFn(browser) {
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );

      // We don't need to wait to confirm the hint hides itelf every time
      let forceClosePopup = true;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      // Make the 2nd field use a generated password
      info("Using contextmenu to fill with a generated password");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      info("waiting for addLogin");
      await storageChangedPromise;
      // Check properties of the newly auto-saved login
      await verifyLogins([
        null, // ignore the first one
        null, // ignore the 2nd one
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);

      info("Waiting to openAndVerifyDoorhanger");
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        passwordLength: LoginTestUtils.generation.LENGTH,
      });
      // remove notification so we can unambiguously check no new notification gets created later
      await cleanupDoorhanger(notif);

      info("Waiting to verifyGeneratedPasswordWasFilled");
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      let { timeLastUsed } = (await Services.logins.getAllLogins())[2];

      info("waiting for submitForm");
      await submitForm(browser);

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      await verifyLogins([
        null, // ignore the first one
        null, // ignore the 2nd one
        {
          timesUsed: 2,
          usedSince: timeLastUsed,
        },
      ]);
      // Check no new doorhanger was shown
      notif = getCaptureDoorhanger("password-change");
      Assert.ok(!notif, "No new doorhanger should be shown");
      await cleanupDoorhanger(); // cleanup for next test
    }
  );
});

add_task(
  async function autosaved_login_updated_to_existing_login_via_doorhanger() {
    // test when filling with a generated password and editing the username in the
    // doorhanger to match an existing login:
    // * the matching login should be updated
    // * the auto-saved login should be deleted
    // * the metadata for the matching login should be updated
    // * the by-origin cache for the password should point at the updated login
    await setup_withOneLogin("user1", "xyzpassword");
    await LoginTestUtils.addLogin({
      username: "user2",
      password: "abcpassword",
    });
    await openFormInNewTab(
      TEST_ORIGIN + FORM_PAGE_PATH,
      {
        password: {
          selector: passwordInputSelector,
          expectedValue: "",
        },
        username: {
          selector: usernameInputSelector,
          expectedValue: "",
        },
      },
      async function taskFn(browser) {
        await SimpleTest.promiseFocus(browser.ownerGlobal);

        let storageChangedPromise = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (_, data) => data == "addLogin"
        );
        // We don't need to wait to confirm the hint hides itelf every time
        let forceClosePopup = true;
        let hintShownAndVerified = verifyConfirmationHint(
          browser,
          forceClosePopup
        );

        info("waiting to fill generated password using context menu");
        await doFillGeneratedPasswordContextMenuItem(
          browser,
          passwordInputSelector
        );

        info("waiting for dismissed password-change notification");
        await waitForDoorhanger(browser, "password-change");

        // Make sure confirmation hint was shown
        info("waiting for verifyConfirmationHint");
        await hintShownAndVerified;

        info("waiting for addLogin");
        await storageChangedPromise;
        info("addLogin promise resolved");
        // Check properties of the newly auto-saved login
        let [user1LoginSnapshot, unused, autoSavedLogin] = await verifyLogins([
          null, // ignore the first one
          null, // ignore the 2nd one
          {
            timesUsed: 1,
            username: "",
            passwordLength: LoginTestUtils.generation.LENGTH,
          },
        ]);
        info("user1LoginSnapshot, guid: " + user1LoginSnapshot.guid);
        info("unused, guid: " + unused.guid);
        info("autoSavedLogin, guid: " + autoSavedLogin.guid);

        info("verifyLogins ok");
        let passwordCacheEntry =
          LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
            "https://example.com"
          );

        Assert.ok(
          passwordCacheEntry,
          "Got the cached generated password entry for https://example.com"
        );
        Assert.equal(
          passwordCacheEntry.value,
          autoSavedLogin.password,
          "Cached password matches the auto-saved login password"
        );
        Assert.equal(
          passwordCacheEntry.storageGUID,
          autoSavedLogin.guid,
          "Cached password guid matches the auto-saved login guid"
        );

        info("Waiting to openAndVerifyDoorhanger");
        let notif = await openAndVerifyDoorhanger(browser, "password-change", {
          dismissed: true,
          anchorExtraAttr: "attention",
          usernameValue: "",
          password: autoSavedLogin.password,
        });
        Assert.ok(notif, "Got password-change notification");

        info("Calling updateDoorhangerInputValues");
        await updateDoorhangerInputValues({
          username: "user1",
        });
        info("doorhanger inputs updated");

        let loginModifiedPromise = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (subject, data) => {
            if (data == "modifyLogin") {
              info("passwordmgr-storage-changed, action: " + data);
              info("subject: " + JSON.stringify(subject));
              return true;
            }
            return false;
          }
        );
        let loginRemovedPromise = TestUtils.topicObserved(
          "passwordmgr-storage-changed",
          (subject, data) => {
            if (data == "removeLogin") {
              info("passwordmgr-storage-changed, action: " + data);
              info("subject: " + JSON.stringify(subject));
              return true;
            }
            return false;
          }
        );

        let promiseHidden = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popuphidden"
        );
        info("clicking change button");
        clickDoorhangerButton(notif, CHANGE_BUTTON);
        await promiseHidden;

        info("Waiting for modifyLogin promise");
        await loginModifiedPromise;

        info("Waiting for removeLogin promise");
        await loginRemovedPromise;

        info("storage-change promises resolved");
        // Check the auto-saved login was removed and the original login updated
        await verifyLogins([
          {
            username: "user1",
            password: autoSavedLogin.password,
            timeCreated: user1LoginSnapshot.timeCreated,
            timeLastUsed: user1LoginSnapshot.timeLastUsed,
            passwordChangedSince: autoSavedLogin.timePasswordChanged,
          },
          null, // ignore user2
        ]);

        // Check we have no notifications at this point
        Assert.ok(!PopupNotifications.isPanelOpen, "No doorhanger is open");
        Assert.ok(
          !PopupNotifications.getNotification("password", browser),
          "No notifications"
        );

        // make sure the cache entry is unchanged with the removal of the auto-saved login
        Assert.equal(
          autoSavedLogin.password,
          LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
            "https://example.com"
          ).value,
          "Generated password cache entry has the expected password value"
        );
      }
    );
  }
);

add_task(async function autosaved_login_updated_to_existing_login_onsubmit() {
  // test when selecting auto-saved generated password in a form filled with an
  // existing login and submitting the form:
  // * the matching login should be updated
  // * the auto-saved login should be deleted
  // * the metadata for the matching login should be updated
  // * the by-origin cache for the password should point at the updated login

  // clear both fields which should be autofilled with our single login
  await setup_withOneLogin("user1", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "user1",
        setValue: "",
        // with an empty password value, no message is sent for a username change
        expectedMessage: "",
      },
    },
    async function taskFn(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      // first, create an auto-saved login with generated password
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      // We don't need to wait to confirm the hint hides itelf every time
      let forceClosePopup = true;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      info("waiting to fill generated password using context menu");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      info("waiting for addLogin");
      await storageChangedPromise;
      info("addLogin promise resolved");
      // Check properties of the newly auto-saved login
      let [user1LoginSnapshot, autoSavedLogin] = await verifyLogins([
        null, // ignore the first one
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);
      info("user1LoginSnapshot, guid: " + user1LoginSnapshot.guid);
      info("autoSavedLogin, guid: " + autoSavedLogin.guid);

      info("verifyLogins ok");
      let passwordCacheEntry =
        LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
          "https://example.com"
        );

      Assert.ok(
        passwordCacheEntry,
        "Got the cached generated password entry for https://example.com"
      );
      Assert.equal(
        passwordCacheEntry.value,
        autoSavedLogin.password,
        "Cached password matches the auto-saved login password"
      );
      Assert.equal(
        passwordCacheEntry.storageGUID,
        autoSavedLogin.guid,
        "Cached password guid matches the auto-saved login guid"
      );

      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        password: autoSavedLogin.password,
      });
      await cleanupDoorhanger(notif);

      // now update and submit the form with the user1 username and the generated password
      info(`submitting form`);
      let submitResults = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#form-basic-username": "user1",
        }
      );
      Assert.equal(
        submitResults.username,
        "user1",
        "Form submitted with expected username"
      );
      Assert.equal(
        submitResults.password,
        autoSavedLogin.password,
        "Form submitted with expected password"
      );
      info(
        `form was submitted, got username/password ${submitResults.username}/${submitResults.password}`
      );

      await waitForDoorhanger(browser, "password-change");
      notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "user1",
        password: autoSavedLogin.password,
      });

      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      let loginModifiedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => {
          if (data == "modifyLogin") {
            info("passwordmgr-storage-changed, action: " + data);
            info("subject: " + JSON.stringify(_));
            return true;
          }
          return false;
        }
      );
      let loginRemovedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => {
          if (data == "removeLogin") {
            info("passwordmgr-storage-changed, action: " + data);
            info("subject: " + JSON.stringify(_));
            return true;
          }
          return false;
        }
      );

      info("clicking change button");
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin promise");
      await loginModifiedPromise;

      info("Waiting for removeLogin promise");
      await loginRemovedPromise;

      info("storage-change promises resolved");
      // Check the auto-saved login was removed and the original login updated
      await verifyLogins([
        {
          username: "user1",
          password: autoSavedLogin.password,
          timeCreated: user1LoginSnapshot.timeCreated,
          timeLastUsed: user1LoginSnapshot.timeLastUsed,
          passwordChangedSince: autoSavedLogin.timePasswordChanged,
        },
      ]);

      // Check we have no notifications at this point
      Assert.ok(!PopupNotifications.isPanelOpen, "No doorhanger is open");
      Assert.ok(
        !PopupNotifications.getNotification("password", browser),
        "No notifications"
      );

      // make sure the cache entry is unchanged with the removal of the auto-saved login
      Assert.equal(
        autoSavedLogin.password,
        LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
          "https://example.com"
        ).value,
        "Generated password cache entry has the expected password value"
      );
    }
  );
});

add_task(async function form_change_from_autosaved_login_to_existing_login() {
  // test when changing from a generated password in a form to an existing saved login
  // * the auto-saved login should not be deleted
  // * the metadata for the matching login should be updated
  // * the by-origin cache for the password should point at the autosaved login

  await setup_withOneLogin("user1", "xyzpassword");
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {
      password: {
        selector: passwordInputSelector,
        expectedValue: "xyzpassword",
        setValue: "",
        expectedMessage: "PasswordEditedOrGenerated",
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "user1",
        setValue: "",
        // with an empty password value, no message is sent for a username change
        expectedMessage: "",
      },
    },
    async function taskFn(browser) {
      await SimpleTest.promiseFocus(browser);

      // first, create an auto-saved login with generated password
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      // We don't need to wait to confirm the hint hides itelf every time
      let forceClosePopup = true;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      info("Filling generated password from AC menu");
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      info("waiting for addLogin");
      await storageChangedPromise;
      info("addLogin promise resolved");
      // Check properties of the newly auto-saved login
      let [user1LoginSnapshot, autoSavedLogin] = await verifyLogins([
        null, // ignore the first one
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);
      info("user1LoginSnapshot, guid: " + user1LoginSnapshot.guid);
      info("autoSavedLogin, guid: " + autoSavedLogin.guid);

      info("verifyLogins ok");
      let passwordCacheEntry =
        LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
          "https://example.com"
        );

      Assert.ok(
        passwordCacheEntry,
        "Got the cached generated password entry for https://example.com"
      );
      Assert.equal(
        passwordCacheEntry.value,
        autoSavedLogin.password,
        "Cached password matches the auto-saved login password"
      );
      Assert.equal(
        passwordCacheEntry.storageGUID,
        autoSavedLogin.guid,
        "Cached password guid matches the auto-saved login guid"
      );

      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        password: autoSavedLogin.password,
      });

      // close but don't remove the doorhanger, we want to ensure it is updated/replaced on further form edits
      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      let PN = notif.owner;
      PN.panel.hidePopup();
      await promiseHidden;
      await TestUtils.waitForTick();

      // now update the form with the user1 username and password
      info(`updating form`);
      let passwordEditedMessages = listenForTestNotification(
        "PasswordEditedOrGenerated",
        2
      );
      let passwordChangeDoorhangerPromise = waitForDoorhanger(
        browser,
        "password-change"
      );
      let hintDidShow = false;
      let hintPromiseShown = BrowserTestUtils.waitForPopupEvent(
        document.getElementById("confirmation-hint"),
        "shown"
      );
      hintPromiseShown.then(() => (hintDidShow = true));

      info("Entering username and password for the previously saved login");

      await changeContentFormValues(browser, {
        [passwordInputSelector]: user1LoginSnapshot.password,
        [usernameInputSelector]: user1LoginSnapshot.username,
      });
      info(
        "form edited, waiting for test notification of PasswordEditedOrGenerated"
      );

      await passwordEditedMessages;
      info("Resolved listenForTestNotification promise");

      await passwordChangeDoorhangerPromise;
      // wait to ensure there's no confirmation hint
      try {
        await TestUtils.waitForCondition(
          () => {
            return hintDidShow;
          },
          `Waiting for confirmationHint popup`,
          undefined,
          25
        );
      } catch (ex) {
        info("Got expected timeout from the waitForCondition: ", ex);
      } finally {
        Assert.ok(!hintDidShow, "No confirmation hint shown");
      }

      // the previous doorhanger would have old values, verify it was updated/replaced with new values from the form
      notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "",
        usernameValue: user1LoginSnapshot.username,
        passwordLength: user1LoginSnapshot.password.length,
      });
      await cleanupDoorhanger(notif);

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );

      // submit the form to ensure the correct updates are made
      await submitForm(browser);
      info("form submitted, waiting for storage changed");
      await storageChangedPromise;

      // Check the auto-saved login has not changed and only metadata on the original login updated
      await verifyLogins([
        {
          username: "user1",
          password: "xyzpassword",
          timeCreated: user1LoginSnapshot.timeCreated,
          usedSince: user1LoginSnapshot.timeLastUsed,
        },
        {
          username: "",
          password: autoSavedLogin.password,
          timeCreated: autoSavedLogin.timeCreated,
          timeLastUsed: autoSavedLogin.timeLastUsed,
        },
      ]);

      // Check we have no notifications at this point
      Assert.ok(!PopupNotifications.isPanelOpen, "No doorhanger is open");
      Assert.ok(
        !PopupNotifications.getNotification("password", browser),
        "No notifications"
      );

      // make sure the cache entry is unchanged with the removal of the auto-saved login
      Assert.equal(
        autoSavedLogin.password,
        LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().get(
          "https://example.com"
        ).value,
        "Generated password cache entry has the expected password value"
      );
    }
  );
});

add_task(async function form_edit_username_and_password_of_generated_login() {
  // test when changing the username and then the password in a form with a generated password (bug 1625242)
  // * the toast is not shown for the username change as the auto-saved login is not modified
  // * the dismissed doorhanger for the username change has the correct username and password
  // * the toast is shown for the change to the generated password
  // * the dismissed doorhanger for the password change has the correct username and password

  await setup_withNoLogins();
  await openFormInNewTab(
    TEST_ORIGIN + FORM_PAGE_PATH,
    {},
    async function taskFn(browser) {
      await SimpleTest.promiseFocus(browser);

      // first, create an auto-saved login with generated password
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      // We don't need to wait to confirm the hint hides itelf every time
      let forceClosePopup = true;
      let hintShownAndVerified = verifyConfirmationHint(
        browser,
        forceClosePopup
      );

      info("Filling generated password from context menu");
      // there's no new-password field in this form so we'll use the context menu
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");

      // Make sure confirmation hint was shown
      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;

      info("waiting for addLogin");
      await storageChangedPromise;
      info("addLogin promise resolved");
      // Check properties of the newly auto-saved login
      let [autoSavedLoginSnapshot] = await verifyLogins([
        {
          timesUsed: 1,
          username: "",
          passwordLength: LoginTestUtils.generation.LENGTH,
        },
      ]);

      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        password: autoSavedLoginSnapshot.password,
      });

      // close but don't remove the doorhanger, we want to ensure it is updated/replaced on further form edits
      let promiseHidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      let PN = notif.owner;
      PN.panel.hidePopup();
      await promiseHidden;
      await TestUtils.waitForTick();

      // change the username then the password in the form
      for (let {
        fieldSelector,
        fieldValue,
        expectedConfirmation,
        expectedDoorhangerUsername,
        expectedDoorhangerPassword,
        expectedDoorhangerType,
      } of [
        {
          fieldSelector: usernameInputSelector,
          fieldValue: "someuser",
          expectedConfirmation: false,
          expectedDoorhangerUsername: "someuser",
          expectedDoorhangerPassword: autoSavedLoginSnapshot.password,
          expectedDoorhangerType: "password-change",
        },
        {
          fieldSelector: passwordInputSelector,
          fieldValue: "!!",
          expectedConfirmation: true,
          expectedDoorhangerUsername: "someuser",
          expectedDoorhangerPassword: autoSavedLoginSnapshot.password + "!!",
          expectedDoorhangerType: "password-change",
        },
      ]) {
        let loginModifiedPromise = expectedConfirmation
          ? TestUtils.topicObserved(
              "passwordmgr-storage-changed",
              (_, data) => data == "modifyLogin"
            )
          : Promise.resolve();

        // now edit the field value
        let passwordEditedMessage = listenForTestNotification(
          "PasswordEditedOrGenerated"
        );
        let passwordChangeDoorhangerPromise = waitForDoorhanger(
          browser,
          expectedDoorhangerType
        );
        let hintDidShow = false;
        let hintPromiseShown = BrowserTestUtils.waitForPopupEvent(
          document.getElementById("confirmation-hint"),
          "shown"
        );
        hintPromiseShown.then(() => (hintDidShow = true));

        info(`updating form: ${fieldSelector}: ${fieldValue}`);
        await appendContentInputvalue(browser, fieldSelector, fieldValue);
        info(
          "form edited, waiting for test notification of PasswordEditedOrGenerated"
        );
        await passwordEditedMessage;
        info(
          "Resolved listenForTestNotification promise, waiting for doorhanger"
        );
        await passwordChangeDoorhangerPromise;
        // wait for possible confirmation hint
        try {
          info("Waiting for hintDidShow");
          await TestUtils.waitForCondition(
            () => hintDidShow,
            `Waiting for confirmationHint popup`,
            undefined,
            25
          );
        } catch (ex) {
          info("Got expected timeout from the waitForCondition: " + ex);
        } finally {
          info("confirmationHint check done, assert on hintDidShow");
          Assert.equal(
            hintDidShow,
            expectedConfirmation,
            "Confirmation hint shown"
          );
        }
        info(
          "Waiting for loginModifiedPromise, expectedConfirmation? " +
            expectedConfirmation
        );
        await loginModifiedPromise;

        // the previous doorhanger would have old values, verify it was updated/replaced with new values from the form
        info("Verifying the doorhanger");
        notif = await openAndVerifyDoorhanger(browser, "password-change", {
          dismissed: true,
          anchorExtraAttr: expectedConfirmation ? "attention" : "",
          usernameValue: expectedDoorhangerUsername,
          passwordLength: expectedDoorhangerPassword.length,
        });
        await cleanupDoorhanger(notif);
      }

      // submit the form to verify we still get the right doorhanger values
      let passwordChangeDoorhangerPromise = waitForDoorhanger(
        browser,
        "password-change"
      );
      await submitForm(browser);
      info("form submitted, waiting for doorhanger");
      await passwordChangeDoorhangerPromise;
      notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: false,
        anchorExtraAttr: "",
        usernameValue: "someuser",
        passwordLength: LoginTestUtils.generation.LENGTH + 2,
      });
      await cleanupDoorhanger(notif);
    }
  );
});
