/*
 * Test the password manager context menu.
 */

/* eslint no-shadow:"off" */

"use strict";

// The hostname for the test URIs.
const TEST_HOSTNAME = "https://example.com";
const MULTIPLE_FORMS_PAGE_PATH = "/browser/toolkit/components/passwordmgr/test/browser/multiple_forms.html";

const CONTEXT_MENU = document.getElementById("contentAreaContextMenu");
const POPUP_HEADER = document.getElementById("fill-login");

/**
 * Initialize logins needed for the tests and disable autofill
 * for login forms for easier testing of manual fill.
 */
add_task(async function test_initialize() {
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
 * Check if the context menu is populated with the right
 * menuitems for the target password input field.
 */
add_task(async function test_context_menu_populate_password_noSchemeUpgrades() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", false);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, async function(browser) {
    await openPasswordContextMenu(browser, "#test-password-1");

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu, 2);

    CONTEXT_MENU.hidePopup();
  });
});

/**
 * Check if the context menu is populated with the right
 * menuitems for the target password input field.
 */
add_task(async function test_context_menu_populate_password_schemeUpgrades() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, async function(browser) {
    await openPasswordContextMenu(browser, "#test-password-1");

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu, 3);

    CONTEXT_MENU.hidePopup();
  });
});

/**
 * Check if the context menu is populated with the right menuitems
 * for the target username field with a password field present.
 */
add_task(async function test_context_menu_populate_username_with_password_noSchemeUpgrades() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", false);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + "/browser/toolkit/components/" +
         "passwordmgr/test/browser/multiple_forms.html",
  }, async function(browser) {
    await openPasswordContextMenu(browser, "#test-username-2");

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu, 2);

    CONTEXT_MENU.hidePopup();
  });
});
/**
 * Check if the context menu is populated with the right menuitems
 * for the target username field with a password field present.
 */
add_task(async function test_context_menu_populate_username_with_password_schemeUpgrades() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + "/browser/toolkit/components/" +
         "passwordmgr/test/browser/multiple_forms.html",
  }, async function(browser) {
    await openPasswordContextMenu(browser, "#test-username-2");

    // Check the content of the password manager popup
    let popupMenu = document.getElementById("fill-login-popup");
    checkMenu(popupMenu, 3);

    CONTEXT_MENU.hidePopup();
  });
});

/**
 * Check if the password field is correctly filled when one
 * login menuitem is clicked.
 */
add_task(async function test_context_menu_password_fill() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, async function(browser) {
    let formDescriptions = await ContentTask.spawn(browser, {}, async function() {
      let forms = Array.from(content.document.getElementsByClassName("test-form"));
      return forms.map((f) => f.getAttribute("description"));
    });

    for (let description of formDescriptions) {
      info("Testing form: " + description);

      let passwordInputIds = await ContentTask.spawn(browser, {description}, async function({description}) {
        let formElement = content.document.querySelector(`[description="${description}"]`);
        let passwords = Array.from(formElement.querySelectorAll("input[type='password']"));
        return passwords.map((p) => p.id);
      });

      for (let inputId of passwordInputIds) {
        info("Testing password field: " + inputId);

        // Synthesize a right mouse click over the username input element.
        await openPasswordContextMenu(browser, "#" + inputId, async function() {
          let inputDisabled = await ContentTask
            .spawn(browser, {inputId}, async function({inputId}) {
              let input = content.document.getElementById(inputId);
              return input.disabled || input.readOnly;
            });

          // If the password field is disabled or read-only, we want to see
          // the disabled Fill Password popup header.
          if (inputDisabled) {
            Assert.ok(!POPUP_HEADER.hidden, "Popup menu is not hidden.");
            Assert.ok(POPUP_HEADER.disabled, "Popup menu is disabled.");
            CONTEXT_MENU.hidePopup();
          }

          return !inputDisabled;
        });

        if (CONTEXT_MENU.state != "open") {
          continue;
        }

        // The only field affected by the password fill
        // should be the target password field itself.
        await assertContextMenuFill(browser, description, null, inputId, 1);
        await ContentTask.spawn(browser, {inputId}, async function({inputId}) {
          let passwordField = content.document.getElementById(inputId);
          Assert.equal(passwordField.value, "password1", "Check upgraded login was actually used");
        });

        CONTEXT_MENU.hidePopup();
      }
    }
  });
});

/**
 * Check if the form is correctly filled when one
 * username context menu login menuitem is clicked.
 */
add_task(async function test_context_menu_username_login_fill() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", true);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, async function(browser) {
    let formDescriptions = await ContentTask.spawn(browser, {}, async function() {
      let forms = Array.from(content.document.getElementsByClassName("test-form"));
      return forms.map((f) => f.getAttribute("description"));
    });

    for (let description of formDescriptions) {
      info("Testing form: " + description);
      let usernameInputIds = await ContentTask
        .spawn(browser, {description}, async function({description}) {
          let formElement = content.document.querySelector(`[description="${description}"]`);
          let inputs = Array.from(formElement.querySelectorAll("input[type='text']"));
          return inputs.map((p) => p.id);
        });

      for (let inputId of usernameInputIds) {
        info("Testing username field: " + inputId);

        // Synthesize a right mouse click over the username input element.
        await openPasswordContextMenu(browser, "#" + inputId, async function() {
          let headerHidden = POPUP_HEADER.hidden;
          let headerDisabled = POPUP_HEADER.disabled;

          let data = {description, inputId, headerHidden, headerDisabled};
          let shouldContinue = await ContentTask.spawn(browser, data, async function(data) {
            let {description, inputId, headerHidden, headerDisabled} = data;
            let formElement = content.document.querySelector(`[description="${description}"]`);
            let usernameField = content.document.getElementById(inputId);
            // We always want to check if the first password field is filled,
            // since this is the current behavior from the _fillForm function.
            let passwordField = formElement.querySelector("input[type='password']");

            // If we don't want to see the actual popup menu,
            // check if the popup is hidden or disabled.
            if (!passwordField || usernameField.disabled || usernameField.readOnly ||
                passwordField.disabled || passwordField.readOnly) {
              if (!passwordField) {
                Assert.ok(headerHidden, "Popup menu is hidden.");
              } else {
                Assert.ok(!headerHidden, "Popup menu is not hidden.");
                Assert.ok(headerDisabled, "Popup menu is disabled.");
              }
              return false;
            }
            return true;
          });

          if (!shouldContinue) {
            CONTEXT_MENU.hidePopup();
          }

          return shouldContinue;
        });

        if (CONTEXT_MENU.state != "open") {
          continue;
        }

        let passwordFieldId = await ContentTask
          .spawn(browser, {description}, async function({description}) {
            let formElement = content.document.querySelector(`[description="${description}"]`);
            return formElement.querySelector("input[type='password']").id;
          });

        // We shouldn't change any field that's not the target username field or the first password field
        await assertContextMenuFill(browser, description, inputId, passwordFieldId, 1);

        await ContentTask.spawn(browser, {passwordFieldId}, async function({passwordFieldId}) {
          let passwordField = content.document.getElementById(passwordFieldId);
          if (!passwordField.hasAttribute("expectedFail")) {
            Assert.equal(passwordField.value, "password1", "Check upgraded login was actually used");
          }
        });

        CONTEXT_MENU.hidePopup();
      }
    }
  });
});

/**
 * Check event telemetry is correctly recorded when opening the saved logins / management UI
 * from the context menu
 */
add_task(async function test_context_menu_open_management() {
  Services.prefs.setBoolPref("signon.schemeUpgrades", false);
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_HOSTNAME + MULTIPLE_FORMS_PAGE_PATH,
  }, async function(browser) {
    await openPasswordContextMenu(browser, "#test-password-1");

    gContextMenu.openPasswordManager();
    // wait until the management UI opens
    let dialogWindow = await waitForPasswordManagerDialog();
    info("Management UI dialog was opened");

    TelemetryTestUtils.assertEvents(
      [["pwmgr", "open_management", "contextmenu"]],
      {category: "pwmgr", method: "open_management"});

    dialogWindow.close();
    CONTEXT_MENU.hidePopup();
  });
});

/**
 * Synthesize mouse clicks to open the password manager context menu popup
 * for a target password input element.
 *
 * assertCallback should return true if we should continue or else false.
 */
async function openPasswordContextMenu(browser, passwordInput, assertCallback = null) {
  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(CONTEXT_MENU, "popupshown");

  // Synthesize a right mouse click over the password input element, we have to trigger
  // both events because formfill code relies on this event happening before the contextmenu
  // (which it does for real user input) in order to not show the password autocomplete.
  let eventDetails = {type: "mousedown", button: 2};
  await BrowserTestUtils.synthesizeMouseAtCenter(passwordInput, eventDetails, browser);
  // Synthesize a contextmenu event to actually open the context menu.
  eventDetails = {type: "contextmenu", button: 2};
  await BrowserTestUtils.synthesizeMouseAtCenter(passwordInput, eventDetails, browser);

  await contextMenuShownPromise;

  if (assertCallback) {
    let shouldContinue = await assertCallback();
    if (!shouldContinue) {
      return;
    }
  }

  // Synthesize a mouse click over the fill login menu header.
  let popupShownPromise = BrowserTestUtils.waitForCondition(() => POPUP_HEADER.open);
  EventUtils.synthesizeMouseAtCenter(POPUP_HEADER, {});
  await popupShownPromise;
}

/**
 * Verify that only the expected form fields are filled.
 */
async function assertContextMenuFill(browser, formId, usernameFieldId, passwordFieldId, loginIndex) {
  let popupMenu = document.getElementById("fill-login-popup");
  let unchangedSelector = `[description="${formId}"] input:not(#${passwordFieldId})`;

  if (usernameFieldId) {
    unchangedSelector += `:not(#${usernameFieldId})`;
  }

  await ContentTask.spawn(browser, {unchangedSelector}, async function({unchangedSelector}) {
    let unchangedFields = content.document.querySelectorAll(unchangedSelector);

    // Store the value of fields that should remain unchanged.
    if (unchangedFields.length) {
      for (let field of unchangedFields) {
        field.setAttribute("original-value", field.value);
      }
    }
  });

  // Execute the default command of the specified login menuitem found in the context menu.
  let loginItem = popupMenu.getElementsByClassName("context-login-item")[loginIndex];

  // Find the used login by it's username (Use only unique usernames in this test).
  let {username, password} = getLoginFromUsername(loginItem.label);

  let data = {username, password, usernameFieldId, passwordFieldId, formId, unchangedSelector};
  let continuePromise = ContentTask.spawn(browser, data, async function(data) {
    let {username, password, usernameFieldId, passwordFieldId, formId, unchangedSelector} = data;
    let form = content.document.querySelector(`[description="${formId}"]`);
    await ContentTaskUtils.waitForEvent(form, "input", "Username input value changed");

    if (usernameFieldId) {
      let usernameField = content.document.getElementById(usernameFieldId);

      // If we have an username field, check if it's correctly filled
      if (usernameField.getAttribute("expectedFail") == null) {
        Assert.equal(username, usernameField.value, "Username filled and correct.");
      }
    }

    if (passwordFieldId) {
      let passwordField = content.document.getElementById(passwordFieldId);

      // If we have a password field, check if it's correctly filled
      if (passwordField && passwordField.getAttribute("expectedFail") == null) {
        Assert.equal(password, passwordField.value, "Password filled and correct.");
      }
    }

    let unchangedFields = content.document.querySelectorAll(unchangedSelector);

    // Check that all fields that should not change have the same value as before.
    if (unchangedFields.length) {
      Assert.ok(() => {
        for (let field of unchangedFields) {
          if (field.value != field.getAttribute("original-value")) {
            return false;
          }
        }
        return true;
      }, "Other fields were not changed.");
    }
  });

  loginItem.doCommand();

  return continuePromise;
}

/**
 * Check if every login that matches the page hostname are available at the context menu.
 * @param {Element} contextMenu
 * @param {Number} expectedCount - Number of logins expected in the context menu. Used to ensure
*                                  we continue testing something useful.
 */
function checkMenu(contextMenu, expectedCount) {
  let logins = loginList().filter(login => {
    return LoginHelper.isOriginMatching(login.hostname, TEST_HOSTNAME, {
      schemeUpgrades: Services.prefs.getBoolPref("signon.schemeUpgrades"),
    });
  });
  // Make an array of menuitems for easier comparison.
  let menuitems = [...CONTEXT_MENU.getElementsByClassName("context-login-item")];
  Assert.equal(menuitems.length, expectedCount, "Expected number of menu items");
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
