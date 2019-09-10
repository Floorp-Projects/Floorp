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

async function setup_withOneLogin(username = "username", password = "pass1") {
  // Reset to a single, known login
  Services.logins.removeAllLogins();
  LoginTestUtils.resetGeneratedPasswordsCache();
  let login = await LoginTestUtils.addLogin({ username, password });
  return login;
}

async function setup_withNoLogins() {
  // Reset to a single, known login
  Services.logins.removeAllLogins();
  is(
    Services.logins.getAllLogins().length,
    0,
    "0 logins at the start of the test"
  );
  LoginTestUtils.resetGeneratedPasswordsCache();
}

async function fillGeneratedPasswordFromACPopup(
  browser,
  passwordInputSelector
) {
  let popup = document.getElementById("PopupAutoComplete");
  ok(popup, "Got popup");
  await openACPopup(popup, browser, passwordInputSelector);

  let item = popup.querySelector(`[originaltype="generatedPassword"]`);
  ok(item, "Got generated password richlistitem");

  await TestUtils.waitForCondition(() => {
    return !EventUtils.isHidden(item);
  }, "Waiting for item to become visible");

  let inputEventPromise = ContentTask.spawn(
    browser,
    [passwordInputSelector],
    async function waitForInput(inputSelector) {
      let passwordInput = content.document.querySelector(inputSelector);
      await ContentTaskUtils.waitForEvent(
        passwordInput,
        "input",
        "Password input value changed"
      );
    }
  );
  info("Clicking the generated password AC item");
  EventUtils.synthesizeMouseAtCenter(item, {});
  info("Waiting for the content input value to change");
  await inputEventPromise;
}

async function checkPromptContents(
  anchorElement,
  browser,
  expectedPasswordLength = 0
) {
  let { panel } = PopupNotifications;
  ok(PopupNotifications.isPanelOpen, "Confirm popup is open");
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
  await ContentTask.spawn(
    browser,
    [passwordInputSelector],
    function checkFinalFieldValue(inputSelector) {
      let { LoginTestUtils: LTU } = ChromeUtils.import(
        "resource://testing-common/LoginTestUtils.jsm"
      );
      let passwordInput = content.document.querySelector(inputSelector);
      is(
        passwordInput.value.length,
        LTU.generation.LENGTH,
        "Password field was filled with generated password"
      );
    }
  );
}

async function verifyConfirmationHint(hintElem) {
  info("verifyConfirmationHint");
  info("verifyConfirmationHint, hintPromiseShown resolved");
  is(
    hintElem.anchorNode.id,
    "password-notification-icon",
    "Hint should be anchored on the password notification icon"
  );
  info("verifyConfirmationHint, assertion ok, wait for poopuphidden");
  await BrowserTestUtils.waitForEvent(hintElem, "popuphidden");
  info("verifyConfirmationHint, /popuphidden");
}

async function openFormInNewTab(url, formValues, taskFn) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        formValues,
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
            field.setAttribute("autocomplete", "new-password");
            if (props.hasOwnProperty("expectedValue")) {
              is(
                field.value,
                props.expectedValue,
                "Check autofilled password value"
              );
            }
            if (props.hasOwnProperty("setValue")) {
              let gotInput = ContentTaskUtils.waitForEvent(
                field,
                "input",
                "field value changed"
              );
              field.setUserInput(props.setValue);
              await gotInput;
            }
          }
          props = usernameProps;
          if (props) {
            let field = doc.querySelector(props.selector);
            if (props.hasOwnProperty("expectedValue")) {
              is(
                field.value,
                props.expectedValue,
                "Check autofilled username value"
              );
            }
            if (props.hasOwnProperty("setValue")) {
              let gotInput = ContentTaskUtils.waitForEvent(
                field,
                "input",
                "field value changed"
              );
              field.setUserInput(props.setValue);
              await gotInput;
            }
          }
        }
      );
      await taskFn(browser);
    }
  );
}

async function waitForDoorhanger(browser, type) {
  await TestUtils.waitForCondition(() => {
    let notif = PopupNotifications.getNotification("password", browser);
    return notif && notif.options.passwordNotificationType == type;
  }, `Waiting for a ${type} notification`);
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
  is(
    passwordValue.length,
    expected.passwordLength || LoginTestUtils.generation.LENGTH,
    "Doorhanger password field has generated 15-char value"
  );
  is(
    usernameValue,
    expected.usernameValue,
    "Doorhanger username field was popuplated"
  );
  return notif;
}

async function hideDoorhangerPopup() {
  info("hideDoorhangerPopup");
  if (!PopupNotifications.isPanelOpen) {
    return;
  }
  let { panel } = PopupNotifications;
  let promiseHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  panel.hidePopup();
  await promiseHidden;
  info("got popuphidden from notification panel");
}

async function submitForm(browser) {
  // Submit the form
  info("Now submit the form with the generated password");

  await ContentTask.spawn(browser, null, async function() {
    content.document.querySelector("form").submit();

    await ContentTaskUtils.waitForCondition(() => {
      return (
        content.location.pathname == "/" &&
        content.document.readyState == "complete"
      );
    }, "Wait for form submission load");
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.generation.available", true],
      ["signon.generation.enabled", true],
    ],
  });
  // assert that there are no logins
  let logins = Services.logins.getAllLogins();
  is(logins.length, 0, "There are no logins");
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
      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      let [{ username, password }] = await storageChangedPromise;
      await verifyGeneratedPasswordWasFilled(browser, passwordInputSelector);
      // Make sure confirmation hint was shown
      await hintPromiseShown;
      await verifyConfirmationHint(confirmationHint);

      // Check properties of the newly auto-saved login
      is(username, "", "Saved login should have no username");
      is(
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
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if the extraAttr attribute was removed"
      );
      await cleanupDoorhanger(notif);

      storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      let [autoSavedLogin] = Services.logins.getAllLogins();
      info("waiting for submitForm");
      await submitForm(browser);
      await storageChangedPromise;
      verifyLogins([
        {
          timesUsed: autoSavedLogin.timesUsed + 1,
          username: "",
        },
      ]);
    }
  );
});

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
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = Services.logins.getAllLogins();
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
      verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
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
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "",
        setValue: "myusername",
      },
    },
    async function taskFn(browser) {
      let [savedLogin] = Services.logins.getAllLogins();
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
      verifyLogins([
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
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = Services.logins.getAllLogins();
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
      verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );
});

add_task(async function autocomplete_generated_password_edited_no_auto_save() {
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
      },
      username: { selector: usernameInputSelector, expectedValue: "" },
    },
    async function taskFn(browser) {
      let [savedLogin] = Services.logins.getAllLogins();
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "modifyLogin"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
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

      verifyLogins([
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
      verifyLogins([
        {
          timesUsed: savedLogin.timesUsed + 1,
          username: "",
        },
      ]);
      await cleanupDoorhanger(notif); // cleanup the doorhanger for next test
    }
  );

  LMP._generatedPasswordsByPrincipalOrigin.clear();
});

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
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "olduser",
        setValue: "differentuser",
      },
    },
    async function taskFn(browser) {
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
      );
      await ContentTask.spawn(
        browser,
        [passwordInputSelector, usernameInputSelector],
        function checkEmptyPasswordField([passwordSelector, usernameSelector]) {
          is(
            content.document.querySelector(passwordSelector).value,
            "",
            "Password field is empty"
          );
        }
      );
      info("waiting to fill generated password using context menu");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );
      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");
      // Make sure confirmation hint was shown
      await hintPromiseShown;
      await verifyConfirmationHint(confirmationHint);

      info("waiting for addLogin");
      await storageChangedPromise;

      // Check properties of the newly auto-saved login
      verifyLogins([
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
      verifyLogins([
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
      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
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
      await hintPromiseShown;
      await verifyConfirmationHint(confirmationHint);

      info("waiting for addLogin");
      await storageChangedPromise;
      // Check properties of the newly auto-saved login
      verifyLogins([
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
      let { timeLastUsed } = Services.logins.getAllLogins()[2];

      info("waiting for submitForm");
      await submitForm(browser);

      info("Waiting for modifyLogin");
      await storageChangedPromise;
      verifyLogins([
        null, // ignore the first one
        null, // ignore the 2nd one
        {
          timesUsed: 2,
          usedSince: timeLastUsed,
        },
      ]);
      // Check no new doorhanger was shown
      notif = getCaptureDoorhanger("password-change");
      ok(!notif, "No new doorhanger should be shown");
      await cleanupDoorhanger(); // cleanup for next test
    }
  );
});

add_task(async function autosaved_login_updated_to_existing_login() {
  // test when filling with a generated password and editing the username in the
  // doorhanger to match an existing login:
  // * the matching login should be updated
  // * the auto-saved login should be deleted
  // * the metadata for the matching login should be updated
  // * the by-origin cache for the password should point at the updated login
  await setup_withOneLogin("user1", "xyzpassword");
  await LoginTestUtils.addLogin({ username: "user2", password: "abcpassword" });
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
      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
      );

      info("waiting to fill generated password using context menu");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");
      // Make sure confirmation hint was shown
      await hintPromiseShown;
      await verifyConfirmationHint(confirmationHint);

      info("waiting for addLogin");
      await storageChangedPromise;
      info("addLogin promise resolved");
      // Check properties of the newly auto-saved login
      let [user1LoginSnapshot, unused, autoSavedLogin] = verifyLogins([
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
      let passwordCacheEntry = LoginManagerParent._generatedPasswordsByPrincipalOrigin.get(
        "https://example.com"
      );

      ok(
        passwordCacheEntry,
        "Got the cached generated password entry for https://example.com"
      );
      is(
        passwordCacheEntry.value,
        autoSavedLogin.password,
        "Cached password matches the auto-saved login password"
      );
      is(
        passwordCacheEntry.storageGUID,
        autoSavedLogin.guid,
        "Cached password guid matches the auto-saved login guid"
      );

      let messagePromise = new Promise(resolve => {
        const eventName = "PasswordManager:onGeneratedPasswordFilledOrEdited";
        browser.messageManager.addMessageListener(
          eventName,
          function mgsHandler(msg) {
            if (msg.target != browser) {
              return;
            }
            browser.messageManager.removeMessageListener(eventName, mgsHandler);
            info("Got onGeneratedPasswordFilledOrEdited, resolving");
            // allow LMP to handle the message, then resolve
            SimpleTest.executeSoon(resolve);
          }
        );
      });

      info("Waiting to openAndVerifyDoorhanger");
      // also moves focus, producing another onGeneratedPasswordFilledOrEdited message from content
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        password: autoSavedLogin.password,
      });
      ok(notif, "Got password-change notification");

      // content sends a 2nd message when we blur the password field,
      // wait for that before interacting with doorhanger
      info("waiting for messagePromise");
      await messagePromise;

      info("Calling updateDoorhangerInputValues");
      await updateDoorhangerInputValues({
        username: "user1",
      });
      info("doorhanger inputs updated");

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
      verifyLogins([
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
      ok(!PopupNotifications.isPanelOpen, "No doorhanger is open");
      ok(
        !PopupNotifications.getNotification("password", browser),
        "No notifications"
      );

      // make sure the cache entry was removed with the removal of the auto-saved login
      ok(
        !LoginManagerParent._generatedPasswordsByPrincipalOrigin.has(
          "https://example.com"
        ),
        "Generated password cache entry has been removed"
      );
    }
  );
});

add_task(async function autosaved_login_updated_to_existing_login() {
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
      },
      username: {
        selector: usernameInputSelector,
        expectedValue: "user1",
        setValue: "",
      },
    },
    async function taskFn(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      // first, create an auto-saved login with generated password
      let storageChangedPromise = TestUtils.topicObserved(
        "passwordmgr-storage-changed",
        (_, data) => data == "addLogin"
      );
      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
      );

      info("waiting to fill generated password using context menu");
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      info("waiting for dismissed password-change notification");
      await waitForDoorhanger(browser, "password-change");
      // Make sure confirmation hint was shown
      await hintPromiseShown;
      await verifyConfirmationHint(confirmationHint);

      info("waiting for addLogin");
      await storageChangedPromise;
      info("addLogin promise resolved");
      // Check properties of the newly auto-saved login
      let [user1LoginSnapshot, autoSavedLogin] = verifyLogins([
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
      let passwordCacheEntry = LoginManagerParent._generatedPasswordsByPrincipalOrigin.get(
        "https://example.com"
      );

      ok(
        passwordCacheEntry,
        "Got the cached generated password entry for https://example.com"
      );
      is(
        passwordCacheEntry.value,
        autoSavedLogin.password,
        "Cached password matches the auto-saved login password"
      );
      is(
        passwordCacheEntry.storageGUID,
        autoSavedLogin.guid,
        "Cached password guid matches the auto-saved login guid"
      );

      let messagePromise = new Promise(resolve => {
        const eventName = "PasswordManager:onGeneratedPasswordFilledOrEdited";
        browser.messageManager.addMessageListener(
          eventName,
          function mgsHandler(msg) {
            if (msg.target != browser) {
              return;
            }
            browser.messageManager.removeMessageListener(eventName, mgsHandler);
            info("Got onGeneratedPasswordFilledOrEdited, resolving");
            // allow LMP to handle the message, then resolve
            SimpleTest.executeSoon(resolve);
          }
        );
      });

      info("Waiting to openAndVerifyDoorhanger");
      // also moves focus, producing another onGeneratedPasswordFilledOrEdited message from content
      let notif = await openAndVerifyDoorhanger(browser, "password-change", {
        dismissed: true,
        anchorExtraAttr: "attention",
        usernameValue: "",
        password: autoSavedLogin.password,
      });
      ok(notif, "Got password-change notification");

      // content sends a 2nd message when we blur the password field,
      // wait for that before interacting with doorhanger
      info("waiting for messagePromise");
      await messagePromise;

      info("Hiding popup.");
      await cleanupDoorhanger(notif);
      info("/Hiding popup.");

      // now submit the form with the user1 username and the generated password
      let doorhangerShown = waitForDoorhanger(browser, "password-change");

      // check and ok the password-change doorhanger
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

      // submit the form with the generated password and username set to user1
      info(`submitting form`);
      let submitResults = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#form-basic-username": "user1",
        }
      );
      is(
        submitResults.username,
        "user1",
        "Form submitted with expected username"
      );
      is(
        submitResults.password,
        autoSavedLogin.password,
        "Form submitted with expected password"
      );
      info(
        `form was submitted, got username/password ${submitResults.username}/${
          submitResults.password
        }`
      );

      await doorhangerShown;
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
      info("clicking change button");
      clickDoorhangerButton(notif, CHANGE_BUTTON);
      await promiseHidden;

      info("Waiting for modifyLogin promise");
      await loginModifiedPromise;

      info("Waiting for removeLogin promise");
      await loginRemovedPromise;

      info("storage-change promises resolved");
      // Check the auto-saved login was removed and the original login updated
      verifyLogins([
        {
          username: "user1",
          password: autoSavedLogin.password,
          timeCreated: user1LoginSnapshot.timeCreated,
          timeLastUsed: user1LoginSnapshot.timeLastUsed,
          passwordChangedSince: autoSavedLogin.timePasswordChanged,
        },
      ]);

      // Check we have no notifications at this point
      ok(!PopupNotifications.isPanelOpen, "No doorhanger is open");
      ok(
        !PopupNotifications.getNotification("password", browser),
        "No notifications"
      );

      // make sure the cache entry was removed with the removal of the auto-saved login
      ok(
        !LoginManagerParent._generatedPasswordsByPrincipalOrigin.has(
          "https://example.com"
        ),
        "Generated password cache entry has been removed"
      );
    }
  );
});
