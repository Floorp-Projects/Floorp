/**
 * Test the password manager context menu item can fill password fields with a generated password.
 */

/* eslint no-shadow:"off" */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const FORM_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const CONTEXT_MENU = document.getElementById("contentAreaContextMenu");

const passwordInputSelector = "#form-basic-password";

registerCleanupFunction(async function cleanup_resetPrefs() {
  await SpecialPowers.popPrefEnv();
});

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

add_task(async function fill_generated_password_hidden() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.generation.available", true],
      ["signon.generation.enabled", false],
    ],
  });

  // test that the generated password option is not present when the feature is not enabled
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      await openPasswordContextMenu(browser, passwordInputSelector);

      let loginPopup = document.getElementById("fill-login-popup");
      let generatedPasswordItem = document.getElementById(
        "fill-login-generated-password"
      );
      let generatedPasswordSeparator = document.getElementById(
        "generated-password-separator"
      );

      // Check the content of the password manager popup
      ok(BrowserTestUtils.is_visible(loginPopup), "popup is visible");
      ok(
        !BrowserTestUtils.is_visible(generatedPasswordItem),
        "generated password item is hidden"
      );
      ok(
        !BrowserTestUtils.is_visible(generatedPasswordSeparator),
        "separator is hidden"
      );

      CONTEXT_MENU.hidePopup();
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function fill_generated_password_empty_field() {
  // test that we can fill with generated password into an empty password field
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
        function checkInitialFieldValue(inputSelector) {
          const input = content.document.querySelector(inputSelector);
          is(input.value.length, 0, "Password field is empty");
          is(
            content.getComputedStyle(input).filter,
            "none",
            "Password field should not be highlighted"
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
          let { LoginTestUtils: LTU } = ChromeUtils.import(
            "resource://testing-common/LoginTestUtils.jsm"
          );
          const input = content.document.querySelector(inputSelector);
          is(
            input.value.length,
            LTU.generation.LENGTH,
            "Password field was filled with generated password"
          );
          isnot(
            content.getComputedStyle(input).filter,
            "none",
            "Password field should be highlighted"
          );
          LTU.loginField.checkPasswordMasked(input, false, "after fill");
        }
      );
    }
  );
});

add_task(async function fill_generated_password_nonempty_field() {
  // test that we can fill with generated password into an non-empty password field
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
        function checkInitialFieldValue(inputSelector) {
          const input = content.document.querySelector(inputSelector);
          input.setUserInput("aa");
          is(
            content.getComputedStyle(input).filter,
            "none",
            "Password field should not be highlighted"
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
          let { LoginTestUtils: LTU } = ChromeUtils.import(
            "resource://testing-common/LoginTestUtils.jsm"
          );
          const input = content.document.querySelector(inputSelector);
          is(
            input.value.length,
            LTU.generation.LENGTH,
            "Password field was filled with generated password"
          );
          isnot(
            content.getComputedStyle(input).filter,
            "none",
            "Password field should be highlighted"
          );
          LTU.loginField.checkPasswordMasked(input, false, "after fill");
        }
      );
    }
  );
});

add_task(async function fill_generated_password_with_matching_logins() {
  // test that we can fill a generated password when there are matching logins
  let login = LoginTestUtils.testData.formLogin({
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
    username: "username",
    password: "pass1",
  });
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  Services.logins.addLogin(login);
  await storageChangedPromised;

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
        function checkInitialFieldValue(inputSelector) {
          is(
            content.document.querySelector(inputSelector).value,
            "pass1",
            "Password field has initial value"
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
          let { LoginTestUtils: LTU } = ChromeUtils.import(
            "resource://testing-common/LoginTestUtils.jsm"
          );
          const input = content.document.querySelector(inputSelector);
          is(
            input.value.length,
            LTU.generation.LENGTH,
            "Password field was filled with generated password"
          );
          isnot(
            content.getComputedStyle(input).filter,
            "none",
            "Password field should be highlighted"
          );
          LTU.loginField.checkPasswordMasked(input, false, "after fill");
        }
      );
    }
  );
});
