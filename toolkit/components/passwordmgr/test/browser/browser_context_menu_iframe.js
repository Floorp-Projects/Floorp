/*
 * Test the password manager context menu.
 */

"use strict";

const TEST_ORIGIN = "https://example.com";

// Test with a page that only has a form within an iframe, not in the top-level document
const IFRAME_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic_iframe.html";

/**
 * Initialize logins needed for the tests and disable autofill
 * for login forms for easier testing of manual fill.
 */
add_task(async function test_initialize() {
  Services.prefs.setBoolPref("signon.autofillForms", false);
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.autofillForms");
    Services.prefs.clearUserPref("signon.schemeUpgrades");
  });
  for (let login of loginList()) {
    Services.logins.addLogin(login);
  }
});

/**
 * Check if the password field is correctly filled when it's in an iframe.
 */
add_task(async function test_context_menu_iframe_fill() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + IFRAME_PAGE_PATH,
    },
    async function(browser) {
      await openPasswordContextMenu(
        browser,
        "#form-basic-password",
        null,
        browser.browsingContext.getChildren()[0]
      );

      let popupMenu = document.getElementById("fill-login-popup");

      // Stores the original value of username
      function promiseFrameInputValue(name) {
        return SpecialPowers.spawn(
          browser.browsingContext.getChildren()[0],
          [name],
          function(inputname) {
            return content.document.getElementById(inputname).value;
          }
        );
      }
      let usernameOriginalValue = await promiseFrameInputValue(
        "form-basic-username"
      );

      // Execute the command of the first login menuitem found at the context menu.
      let passwordChangedPromise = SpecialPowers.spawn(
        browser.browsingContext.getChildren()[0],
        [],
        function(inputname) {
          return new Promise(resolve => {
            let passwordInput = content.document.getElementById(
              "form-basic-password"
            );
            // Cannot pass resolve directly to the event listener, as then
            // spawn will try to return the non-serializable event passed to the listener
            // and generate an error.
            passwordInput.addEventListener(
              "input",
              () => {
                resolve();
              },
              { once: true }
            );
          });
        }
      );

      // Wait a tick for SpecialPowers.spawn to add the input listener.
      await new Promise(resolve => {
        SimpleTest.executeSoon(resolve);
      });

      let firstLoginItem = popupMenu.getElementsByClassName(
        "context-login-item"
      )[0];
      firstLoginItem.doCommand();

      await passwordChangedPromise;

      // Find the used login by it's username.
      let login = getLoginFromUsername(firstLoginItem.label);
      let passwordValue = await promiseFrameInputValue("form-basic-password");
      is(login.password, passwordValue, "Password filled and correct.");

      let usernameNewValue = await promiseFrameInputValue(
        "form-basic-username"
      );
      is(
        usernameOriginalValue,
        usernameNewValue,
        "Username value was not changed."
      );

      let contextMenu = document.getElementById("contentAreaContextMenu");
      contextMenu.hidePopup();
    }
  );
});

/**
 * Check that the login context menu items don't appear on an opaque origin.
 */
add_task(async function test_context_menu_iframe_sandbox() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + IFRAME_PAGE_PATH,
    },
    async function(browser) {
      await openPasswordContextMenu(
        browser,
        "#form-basic-password",
        function checkDisabled() {
          let popupHeader = document.getElementById("fill-login");
          ok(
            popupHeader.hidden,
            "Check that the Fill Login menu item is hidden"
          );
          return false;
        },
        browser.browsingContext.getChildren()[1]
      );
      let contextMenu = document.getElementById("contentAreaContextMenu");
      contextMenu.hidePopup();
    }
  );
});

/**
 * Check that the login context menu item appears for sandbox="allow-same-origin"
 */
add_task(async function test_context_menu_iframe_sandbox_same_origin() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + IFRAME_PAGE_PATH,
    },
    async function(browser) {
      await openPasswordContextMenu(
        browser,
        "#form-basic-password",
        function checkDisabled() {
          let popupHeader = document.getElementById("fill-login");
          ok(
            !popupHeader.hidden,
            "Check that the Fill Login menu item is visible"
          );
          ok(
            !popupHeader.disabled,
            "Check that the Fill Login menu item is disabled"
          );
          return false;
        },
        browser.browsingContext.getChildren()[2]
      );

      let contextMenu = document.getElementById("contentAreaContextMenu");
      contextMenu.hidePopup();
    }
  );
});

/**
 * Search for a login by it's username.
 *
 * Only unique login/origin combinations should be used at this test.
 */
function getLoginFromUsername(username) {
  return loginList().find(login => login.username == username);
}

/**
 * List of logins used for the test.
 *
 * We should only use unique usernames in this test,
 * because we need to search logins by username. There is one duplicate u+p combo
 * in order to test de-duping in the menu.
 */
function loginList() {
  return [
    LoginTestUtils.testData.formLogin({
      origin: "https://example.com",
      formActionOrigin: "https://example.com",
      username: "username",
      password: "password",
    }),
    // Same as above but HTTP in order to test de-duping.
    LoginTestUtils.testData.formLogin({
      origin: "http://example.com",
      formActionOrigin: "http://example.com",
      username: "username",
      password: "password",
    }),
    LoginTestUtils.testData.formLogin({
      origin: "http://example.com",
      formActionOrigin: "http://example.com",
      username: "username1",
      password: "password1",
    }),
    LoginTestUtils.testData.formLogin({
      origin: "https://example.com",
      formActionOrigin: "https://example.com",
      username: "username2",
      password: "password2",
    }),
    LoginTestUtils.testData.formLogin({
      origin: "http://example.org",
      formActionOrigin: "http://example.org",
      username: "username-cross-origin",
      password: "password-cross-origin",
    }),
  ];
}
