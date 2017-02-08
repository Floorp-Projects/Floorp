/*
 * Test the password manager context menu interaction with autocomplete.
 */

"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

var gUnexpectedIsTODO = false;

/**
 * Initialize logins needed for the tests and disable autofill
 * for login forms for easier testing of manual fill.
 */
add_task(function* test_initialize() {
  let autocompletePopup = document.getElementById("PopupAutoComplete");
  Services.prefs.setBoolPref("signon.autofillForms", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.autofillForms");
    autocompletePopup.removeEventListener("popupshowing", autocompleteUnexpectedPopupShowing);
  });
  for (let login of loginList()) {
    Services.logins.addLogin(login);
  }
  autocompletePopup.addEventListener("popupshowing", autocompleteUnexpectedPopupShowing);
});

add_task(function* test_context_menu_username() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + BASIC_FORM_PAGE_PATH,
  }, function* (browser) {
    yield openContextMenu(browser, "#form-basic-username");

    let contextMenu = document.getElementById("contentAreaContextMenu");
    Assert.equal(contextMenu.state, "open", "Context menu opened");
    contextMenu.hidePopup();
  });
});

add_task(function* test_context_menu_password() {
  gUnexpectedIsTODO = true;
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + BASIC_FORM_PAGE_PATH,
  }, function* (browser) {
    yield openContextMenu(browser, "#form-basic-password");

    let contextMenu = document.getElementById("contentAreaContextMenu");
    Assert.equal(contextMenu.state, "open", "Context menu opened");
    contextMenu.hidePopup();
  });
});

function autocompleteUnexpectedPopupShowing(event) {
  if (gUnexpectedIsTODO) {
    todo(false, "Autocomplete shouldn't appear");
  } else {
    Assert.ok(false, "Autocomplete shouldn't appear");
  }
  event.target.hidePopup();
}

/**
 * Synthesize mouse clicks to open the context menu popup
 * for a target login input element.
 */
function* openContextMenu(browser, loginInput) {
  // First synthesize a mousedown. We need this to get the focus event with the "contextmenu" event.
  let eventDetails1 = {type: "mousedown", button: 2};
  BrowserTestUtils.synthesizeMouseAtCenter(loginInput, eventDetails1, browser);

  // Then synthesize the contextmenu click over the input element.
  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(window, "popupshown");
  let eventDetails = {type: "contextmenu", button: 2};
  BrowserTestUtils.synthesizeMouseAtCenter(loginInput, eventDetails, browser);
  yield contextMenuShownPromise;

  // Wait to see which popups are shown.
  yield new Promise(resolve => setTimeout(resolve, 1000));
}

function loginList() {
  return [
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username",
      password: "password",
    }),
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username2",
      password: "password2",
    }),
  ];
}
