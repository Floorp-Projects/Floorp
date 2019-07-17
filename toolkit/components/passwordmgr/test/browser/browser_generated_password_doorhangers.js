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

async function addLogin({ username, password }) {
  const login = LoginTestUtils.testData.formLogin({
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
    username,
    password,
  });
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  Services.logins.addLogin(login);
  await storageChangedPromised;
  return login;
}

let login1;
function addOneLogin() {
  login1 = addLogin({ username: "username", password: "pass1" });
}

function openACPopup(popup, browser, inputSelector) {
  return new Promise(async resolve => {
    let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

    await SimpleTest.promiseFocus(browser);
    info("content window focused");

    // Focus the username field to open the popup.
    await ContentTask.spawn(browser, [inputSelector], function openAutocomplete(
      sel
    ) {
      content.document.querySelector(sel).focus();
    });

    let shown = await promiseShown;
    ok(shown, "autocomplete popup shown");
    resolve(shown);
  });
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

async function checkPromptContents(anchorElement, browser) {
  let { panel } = PopupNotifications;
  let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
  await SimpleTest.promiseFocus(browser);
  info("Clicking on anchor to show popup.");
  anchorElement.click();
  await promiseShown;
  let notificationElement = panel.childNodes[0];
  return {
    passwordValue: notificationElement.querySelector(
      "#password-notification-password"
    ).value,
    usernameValue: notificationElement.querySelector(
      "#password-notification-username"
    ).value,
  };
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
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector, usernameInputSelector],
        function prepareAndCheckForm([passwordSelector, usernameSelector]) {
          let passwordInput = content.document.querySelector(passwordSelector);
          // We'll reuse the form_basic.html, but ensure we'll get the generated password autocomplete option
          passwordInput.setAttribute("autocomplete", "new-password");
          passwordInput.value = "";
          let usernameInput = content.document.querySelector(usernameSelector);
          usernameInput.setUserInput("user1");
        }
      );

      let confirmationHint = document.getElementById("confirmation-hint");
      let hintPromiseShown = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popupshown"
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        function checkFinalFieldValue(inputSelector) {
          let passwordInput = content.document.querySelector(inputSelector);
          is(
            passwordInput.value.length,
            15,
            "Password field was filled with generated password"
          );
        }
      );

      // Make sure confirmation hint was shown
      await hintPromiseShown;

      Assert.equal(
        confirmationHint.anchorNode.id,
        "password-notification-icon",
        "Hint should be anchored on the password notification icon"
      );

      let hintPromiseHidden = BrowserTestUtils.waitForEvent(
        confirmationHint,
        "popuphidden"
      );
      await hintPromiseHidden;

      // check a dismissed prompt was shown with extraAttr attribute
      let notif = getCaptureDoorhanger("password-change");
      ok(notif && notif.dismissed, "Dismissed notification was created");
      is(
        notif.anchorElement.getAttribute("extraAttr"),
        "attention",
        "Check if icon has the extraAttr attribute"
      );

      let { passwordValue, usernameValue } = await checkPromptContents(
        notif.anchorElement,
        browser
      );
      is(
        passwordValue.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      is(usernameValue, "user1", "Doorhanger username field was popuplated");

      info("Hiding popup.");
      let { panel } = PopupNotifications;
      let promiseHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
      panel.hidePopup();
      await promiseHidden;

      // confirm the extraAttr attribute is removed after opening & dismissing the doorhanger
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if the extraAttr attribute was removed"
      );
      notif.remove();
    }
  );
});

add_task(async function setup_logins() {
  // Reset all passwords
  Services.logins.removeAllLogins();
  // ..and use a single matching login for the following tests
  await addOneLogin();
});

add_task(async function contextfill_generated_password_with_matching_logins() {
  // test that we can fill a generated password when there are matching logins
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        async function waitForFilledFieldValue(inputSelector) {
          let passwordInput = content.document.querySelector(inputSelector);
          await ContentTaskUtils.waitForCondition(
            () => passwordInput.value == "pass1",
            "Password field got autofilled value"
          );
        }
      );
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        function checkFinalFieldValue(inputSelector) {
          is(
            content.document.querySelector(inputSelector).value.length,
            15,
            "Password field was filled with generated password"
          );
        }
      );
      // check a dismissed prompt was shown
      let notif = getCaptureDoorhanger("password-save");
      ok(notif && notif.dismissed, "Dismissed notification was created");

      let { passwordValue } = await checkPromptContents(
        notif.anchorElement,
        browser
      );
      is(
        passwordValue.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if icon has the extraAttr attribute"
      );
      notif.remove();
    }
  );
});

add_task(async function contextfill_generated_password_with_username() {
  // test that the prompt resulting from filling with a generated password displays the username
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector, usernameInputSelector],
        function checkAndSetFieldValue([passwordSelector, usernameSelector]) {
          is(
            content.document.querySelector(passwordSelector).value,
            "pass1",
            "Password field has initial autofilled value"
          );
          content.document
            .querySelector(usernameSelector)
            .setUserInput("user1");
        }
      );

      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );

      // check a dismissed prompt was shown
      let notif = getCaptureDoorhanger("password-save");
      ok(notif && notif.dismissed, "Dismissed notification was created");

      let { passwordValue, usernameValue } = await checkPromptContents(
        notif.anchorElement
      );
      is(
        passwordValue.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      is(
        usernameValue,
        "user1",
        "Doorhanger username field has the username field value"
      );
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if icon has the extraAttr attribute"
      );
      notif.remove();
    }
  );
});

add_task(async function autocomplete_generated_password() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector, usernameInputSelector],
        function prepareAndCheckForm([passwordSelector, usernameSelector]) {
          let passwordInput = content.document.querySelector(passwordSelector);
          // We'll reuse the form_basic.html, but ensure we'll get the generated password autocomplete option
          passwordInput.setAttribute("autocomplete", "new-password");
          passwordInput.value = "";
          let usernameInput = content.document.querySelector(usernameSelector);
          usernameInput.setUserInput("user1");
        }
      );
      await fillGeneratedPasswordFromACPopup(browser, passwordInputSelector);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        function checkFinalFieldValue(inputSelector) {
          let passwordInput = content.document.querySelector(inputSelector);
          is(
            passwordInput.value.length,
            15,
            "Password field was filled with generated password"
          );
        }
      );

      // check a dismissed prompt was shown
      let notif = getCaptureDoorhanger("password-save");
      ok(notif && notif.dismissed, "Dismissed notification was created");
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if icon has the extraAttr attribute"
      );

      let { passwordValue, usernameValue } = await checkPromptContents(
        notif.anchorElement,
        browser
      );
      is(
        passwordValue.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      is(usernameValue, "user1", "Doorhanger username field was popuplated");
      notif.remove();
    }
  );
});

add_task(async function password_change_without_username() {
  const CHANGE_FORM_PATH =
    "/browser/toolkit/components/passwordmgr/test/browser/form_password_change.html";
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + CHANGE_FORM_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      // Save 2nd login different from the 1st one
      addLogin({
        username: "username2",
        password: "pass2",
      });

      // Make the 2nd field use a generated password
      await doFillGeneratedPasswordContextMenuItem(browser, "#newpass");

      // check a dismissed prompt was shown
      let notif = getCaptureDoorhanger("password-save");
      ok(notif && notif.dismissed, "Dismissed notification was created");

      let { passwordValue, usernameValue } = await checkPromptContents(
        notif.anchorElement
      );
      is(
        passwordValue.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      is(
        usernameValue,
        "",
        "Doorhanger username field has the username field value"
      );
      ok(
        !notif.anchorElement.hasAttribute("extraAttr"),
        "Check if icon has the extraAttr attribute"
      );
      notif.remove();

      // Submit the form
      await ContentTask.spawn(browser, null, function() {
        content.document.querySelector("#form").submit();
      });

      // Check a non-dismissed prompt was shown
      notif = getCaptureDoorhanger("password-save");
      ok(notif && !notif.dismissed, "Non-dismissed notification was created");

      ok(!EventUtils.isHidden(notif.anchorElement), "Anchor should be shown");
      let {
        passwordValue: passwordValue2,
        usernameValue: usernameValue2,
      } = await checkPromptContents(notif.anchorElement);
      is(
        passwordValue2.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      is(usernameValue2, "", "Doorhanger username field has no value");
      notif.remove();
    }
  );
});
