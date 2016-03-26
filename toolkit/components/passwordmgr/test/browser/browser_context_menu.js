/*
 * Test the password manager context menu.
 */

"use strict";

// The hostname for the test URIs.
const TEST_HOSTNAME = "https://example.com";
const MULTIPLE_FORMS_PAGE_PATH = "/browser/toolkit/components/passwordmgr/test/browser/multiple_forms.html";

/**
 * Initialize logins needed for the tests and disable autofill
 * for login forms for easier testing of manual fill.
 */
add_task(function* test_initialize() {
  Services.prefs.setBoolPref("signon.autofillForms", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.autofillForms");
    Services.logins.removeAllLogins();
  });
  for (let login of loginList()) {
    Services.logins.addLogin(login);
  }
});

/**
 * Check if the context menu is populated with the right
 * menuitems for the target password input field.
 */
add_task(function* test_context_menu_populate_password() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, function* (browser) {
    let passwordInput = browser.contentWindow.document.getElementById("test-password-1");

    yield openPasswordContextMenu(browser, passwordInput);

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu);

    let contextMenu = document.getElementById("contentAreaContextMenu");
    contextMenu.hidePopup();
  });
});

/**
 * Check if the context menu is populated with the right menuitems
 * for the target username field with a password field present.
 */
add_task(function* test_context_menu_populate_username_with_password() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + "/browser/toolkit/components/" +
         "passwordmgr/test/browser/multiple_forms.html",
  }, function* (browser) {
    let passwordInput = browser.contentWindow.document.getElementById("test-username-2");

    yield openPasswordContextMenu(browser, passwordInput);

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu);

    let contextMenu = document.getElementById("contentAreaContextMenu");
    contextMenu.hidePopup();
  });
});

/**
 * Check if the password field is correctly filled when one
 * login menuitem is clicked.
 */
add_task(function* test_context_menu_password_fill() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, function* (browser) {

    let testForms = browser.contentWindow.document.getElementsByClassName("test-form");
    for (let form of testForms) {
      let usernameInputList = form.querySelectorAll("input[type='password']");
      info("Testing form: " + form.getAttribute("description"));

      for (let passwordField of usernameInputList) {
        info("Testing password field: " + passwordField.id);

        let contextMenu = document.getElementById("contentAreaContextMenu");
        let menuItemStatus = form.getAttribute("menuitemStatus");

        // Synthesize a right mouse click over the username input element.
        yield openPasswordContextMenu(browser, passwordField, ()=> {
          let popupHeader = document.getElementById("fill-login");

          // If the password field is disabled or read-only, we want to see
          // the disabled Fill Password popup header.
          if (passwordField.disabled || passwordField.readOnly) {
            Assert.ok(!popupHeader.hidden, "Popup menu is not hidden.");
            Assert.ok(popupHeader.disabled, "Popup menu is disabled.");
            contextMenu.hidePopup();
            return false;
          }
          return true;
        });

        if (contextMenu.state != "open") {
          continue;
        }

        // The only field affected by the password fill
        // should be the target password field itself.
        let unchangedFields = form.querySelectorAll('input:not(#' + passwordField.id + ')');
        yield assertContextMenuFill(form, null, passwordField, unchangedFields);
        contextMenu.hidePopup();
      }
    }
  });
});

/**
 * Check if the form is correctly filled when one
 * username context menu login menuitem is clicked.
 */
add_task(function* test_context_menu_username_login_fill() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, function* (browser) {

    let testForms = browser.contentWindow.document.getElementsByClassName("test-form");
    for (let form of testForms) {
      let usernameInputList = form.querySelectorAll("input[type='text']");
      info("Testing form: " + form.getAttribute("description"));

      for (let usernameField of usernameInputList) {
        info("Testing username field: " + usernameField.id);

        // We always want to check if the first password field is filled,
        // since this is the current behavior from the _fillForm function.
        let passwordField = form.querySelector("input[type='password']");

        let contextMenu = document.getElementById("contentAreaContextMenu");
        let menuItemStatus = form.getAttribute("menuitemStatus");

        // Synthesize a right mouse click over the username input element.
        yield openPasswordContextMenu(browser, usernameField, ()=> {
          let popupHeader = document.getElementById("fill-login");

          // If we don't want to see the actual popup menu,
          // check if the popup is hidden or disabled.
          if (!passwordField || usernameField.disabled || usernameField.readOnly ||
              passwordField.disabled || passwordField.readOnly) {
            if (!passwordField) {
              Assert.ok(popupHeader.hidden, "Popup menu is hidden.");
            } else {
              Assert.ok(!popupHeader.hidden, "Popup menu is not hidden.");
              Assert.ok(popupHeader.disabled, "Popup menu is disabled.");
            }
            contextMenu.hidePopup();
            return false;
          }
          return true;
        });

        if (contextMenu.state != "open") {
          continue;
        }
        // We shouldn't change any field that's not the target username field or the first password field
        let unchangedFields = form.querySelectorAll('input:not(#' + usernameField.id + '):not(#' + passwordField.id + ')');
        yield assertContextMenuFill(form, usernameField, passwordField, unchangedFields);
        contextMenu.hidePopup();
      }
    }
  });
});

/**
 * Check if the password field is correctly filled when it's in an iframe.
 */
add_task(function* test_context_menu_iframe_fill() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, function* (browser) {
    let iframe = browser.contentWindow.document.getElementById("test-iframe");
    let passwordInput = iframe.contentDocument.getElementById("form-basic-password");

    let contextMenuShownPromise = BrowserTestUtils.waitForEvent(window, "popupshown");
    let eventDetails = {type: "contextmenu", button: 2};

    // To click at the right point we have to take into account the iframe offset.
    let iframeRect = iframe.getBoundingClientRect();
    let inputRect = passwordInput.getBoundingClientRect();
    let clickPos = {
      offsetX: iframeRect.left + inputRect.width / 2,
      offsetY: iframeRect.top  + inputRect.height / 2,
    };

    // Synthesize a right mouse click over the password input element.
    BrowserTestUtils.synthesizeMouse(passwordInput, clickPos.offsetX, clickPos.offsetY, eventDetails, browser);
    yield contextMenuShownPromise;

    // Synthesize a mouse click over the fill login menu header.
    let popupHeader = document.getElementById("fill-login");
    let popupShownPromise = BrowserTestUtils.waitForEvent(popupHeader, "popupshown");
    EventUtils.synthesizeMouseAtCenter(popupHeader, {});
    yield popupShownPromise;

    let popupMenu = document.getElementById("fill-login-popup");

    // Stores the original value of username
    let usernameInput = iframe.contentDocument.getElementById("form-basic-username");
    let usernameOriginalValue = usernameInput.value;

    // Execute the command of the first login menuitem found at the context menu.
    let firstLoginItem = popupMenu.getElementsByClassName("context-login-item")[0];
    firstLoginItem.doCommand();

    yield BrowserTestUtils.waitForEvent(passwordInput, "input", "Password input value changed");

    // Find the used login by it's username.
    let login = getLoginFromUsername(firstLoginItem.label);

    Assert.equal(login.password, passwordInput.value, "Password filled and correct.");

    Assert.equal(usernameOriginalValue,
                 usernameInput.value,
                 "Username value was not changed.");

    let contextMenu = document.getElementById("contentAreaContextMenu");
    contextMenu.hidePopup();
  });
});

/**
 * Synthesize mouse clicks to open the password manager context menu popup
 * for a target password input element.
 *
 * assertCallback should return true if we should continue or else false.
 */
function* openPasswordContextMenu(browser, passwordInput, assertCallback = null) {
  // Synthesize a right mouse click over the password input element.
  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(window, "popupshown");
  let eventDetails = {type: "contextmenu", button: 2};
  BrowserTestUtils.synthesizeMouseAtCenter(passwordInput, eventDetails, browser);
  yield contextMenuShownPromise;

  if (assertCallback) {
    if (!assertCallback.call()) {
      return;
    }
  }

  // Synthesize a mouse click over the fill login menu header.
  let popupHeader = document.getElementById("fill-login");
  let popupShownPromise = BrowserTestUtils.waitForEvent(popupHeader, "popupshown");
  EventUtils.synthesizeMouseAtCenter(popupHeader, {});
  yield popupShownPromise;
}

/**
 * Verify that only the expected form fields are filled.
 */
function* assertContextMenuFill(form, usernameField, passwordField, unchangedFields){
  let popupMenu = document.getElementById("fill-login-popup");

  // Store the value of fields that should remain unchanged.
  if (unchangedFields.length) {
    for (let field of unchangedFields) {
      field.setAttribute("original-value", field.value);
    }
  }

  // Execute the default command of the first login menuitem found at the context menu.
  let firstLoginItem = popupMenu.getElementsByClassName("context-login-item")[0];
  firstLoginItem.doCommand();

  yield BrowserTestUtils.waitForEvent(form, "input", "Username input value changed");

  // Find the used login by it's username (Use only unique usernames in this test).
  let login = getLoginFromUsername(firstLoginItem.label);

  // If we have an username field, check if it's correctly filled
  if (usernameField && usernameField.getAttribute("expectedFail") == null) {
    Assert.equal(login.username, usernameField.value, "Username filled and correct.");
  }

  // If we have a password field, check if it's correctly filled
  if (passwordField && passwordField.getAttribute("expectedFail") == null) {
    Assert.equal(passwordField.value, login.password, "Password filled and correct.");
  }

  // Check that all fields that should not change have the same value as before.
  if (unchangedFields.length) {
    Assert.ok(()=> {
      for (let field of unchangedFields) {
        if (field.value != field.getAttribute("original-value")) {
          return false;
        }
      }
      return true;
    }, "Other fields were not changed.");
  }
}

/**
 * Check if every login that matches the page hostname are available at the context menu.
 */
function checkMenu(contextMenu) {
  let logins = loginList().filter(login => login.hostname == TEST_HOSTNAME);
  // Make an array of menuitems for easier comparison.
  let menuitems = [...contextMenu.getElementsByClassName("context-login-item")];
  Assert.equal(menuitems.length, logins.length, "Same amount of menu items and expected logins.");
  Assert.ok(logins.every(l => menuitems.some(m => l.username == m.label)), "Every login have an item at the menu.");
}

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
 * because we need to search logins by username.
 */
function loginList() {
  return [
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
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
      hostname: "https://example.org",
      formSubmitURL: "https://example.org",
      username: "username-cross-origin",
      password: "password-cross-origin",
    }),
  ];
}
