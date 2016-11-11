/*
 * Test the password manager context menu.
 */

"use strict";

const TEST_HOSTNAME = "https://example.com";

// Test with a page that only has a form within an iframe, not in the top-level document
const IFRAME_PAGE_PATH = "/browser/toolkit/components/passwordmgr/test/browser/form_basic_iframe.html";

/**
 * Initialize logins needed for the tests and disable autofill
 * for login forms for easier testing of manual fill.
 */
add_task(function* test_initialize() {
  Services.prefs.setBoolPref("signon.autofillForms", false);
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
add_task(function* test_context_menu_iframe_fill() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + IFRAME_PAGE_PATH
  }, function* (browser) {
    function getPasswordInput() {
      let frame = content.document.getElementById("test-iframe");
      return frame.contentDocument.getElementById("form-basic-password");
    }

    let contextMenuShownPromise = BrowserTestUtils.waitForEvent(window, "popupshown");
    let eventDetails = {type: "contextmenu", button: 2};

    // To click at the right point we have to take into account the iframe offset.
    // Synthesize a right mouse click over the password input element.
    BrowserTestUtils.synthesizeMouseAtCenter(getPasswordInput, eventDetails, browser);
    yield contextMenuShownPromise;

    // Synthesize a mouse click over the fill login menu header.
    let popupHeader = document.getElementById("fill-login");
    let popupShownPromise = BrowserTestUtils.waitForEvent(popupHeader, "popupshown");
    EventUtils.synthesizeMouseAtCenter(popupHeader, {});
    yield popupShownPromise;

    let popupMenu = document.getElementById("fill-login-popup");

    // Stores the original value of username
    function promiseFrameInputValue(name) {
      return ContentTask.spawn(browser, name, function(inputname) {
        let iframe = content.document.getElementById("test-iframe");
        let input = iframe.contentDocument.getElementById(inputname);
        return input.value;
      });
    }
    let usernameOriginalValue = yield promiseFrameInputValue("form-basic-username");

    // Execute the command of the first login menuitem found at the context menu.
    let passwordChangedPromise = ContentTask.spawn(browser, null, function* () {
      let frame = content.document.getElementById("test-iframe");
      let passwordInput = frame.contentDocument.getElementById("form-basic-password");
      yield ContentTaskUtils.waitForEvent(passwordInput, "input");
    });

    let firstLoginItem = popupMenu.getElementsByClassName("context-login-item")[0];
    firstLoginItem.doCommand();

    yield passwordChangedPromise;

    // Find the used login by it's username.
    let login = getLoginFromUsername(firstLoginItem.label);
    let passwordValue = yield promiseFrameInputValue("form-basic-password");
    is(login.password, passwordValue, "Password filled and correct.");

    let usernameNewValue = yield promiseFrameInputValue("form-basic-username");
    is(usernameOriginalValue,
       usernameNewValue,
       "Username value was not changed.");

    let contextMenu = document.getElementById("contentAreaContextMenu");
    contextMenu.hidePopup();
  });
});

/**
 * Search for a login by it's username.
 *
 * Only unique login/hostname combinations should be used at this test.
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
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username",
      password: "password",
    }),
    // Same as above but HTTP in order to test de-duping.
    LoginTestUtils.testData.formLogin({
      hostname: "http://example.com",
      formSubmitURL: "http://example.com",
      username: "username",
      password: "password",
    }),
    LoginTestUtils.testData.formLogin({
      hostname: "http://example.com",
      formSubmitURL: "http://example.com",
      username: "username1",
      password: "password1",
    }),
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username2",
      password: "password2",
    }),
    LoginTestUtils.testData.formLogin({
      hostname: "http://example.org",
      formSubmitURL: "http://example.org",
      username: "username-cross-origin",
      password: "password-cross-origin",
    }),
  ];
}
