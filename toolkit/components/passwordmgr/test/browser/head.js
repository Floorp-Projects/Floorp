const DIRECTORY_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";

ChromeUtils.import("resource://gre/modules/LoginHelper.jsm", this);
const { LoginManagerParent: LMP } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);
ChromeUtils.import("resource://testing-common/LoginTestUtils.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm", this);

add_task(async function common_initialize() {
  await SpecialPowers.pushPrefEnv({ set: [["signon.rememberSignons", true]] });
});

registerCleanupFunction(
  async function cleanup_removeAllLoginsAndResetRecipes() {
    await SpecialPowers.popPrefEnv();

    LoginTestUtils.clearData();
    LoginTestUtils.resetGeneratedPasswordsCache();
    clearHttpAuths();
    Services.telemetry.clearEvents();

    let recipeParent = LoginTestUtils.recipes.getRecipeParent();
    if (!recipeParent) {
      // No need to reset the recipes if the recipe module wasn't even loaded.
      return;
    }
    await recipeParent.then(recipeParentResult => recipeParentResult.reset());

    await cleanupDoorhanger();
    let notif;
    while ((notif = PopupNotifications.getNotification("password"))) {
      notif.remove();
    }
    await Promise.resolve();
  }
);

/**
 * Compared logins in storage to expected values
 *
 * @param {array} expectedLogins
 *        An array of expected login properties
 * @return {nsILoginInfo[]} - All saved logins sorted by timeCreated
 */
function verifyLogins(expectedLogins = []) {
  let allLogins = Services.logins.getAllLogins();
  allLogins.sort((a, b) => a.timeCreated > b.timeCreated);
  is(
    allLogins.length,
    expectedLogins.length,
    "Check actual number of logins matches the number of provided expected property-sets"
  );
  for (let i = 0; i < expectedLogins.length; i++) {
    // if the test doesn't care about comparing properties for this login, just pass false/null.
    let expected = expectedLogins[i];
    if (expected) {
      let login = allLogins[i];
      if (typeof expected.timesUsed !== "undefined") {
        is(login.timesUsed, expected.timesUsed, "Check timesUsed");
      }
      if (typeof expected.passwordLength !== "undefined") {
        is(
          login.password.length,
          expected.passwordLength,
          "Check passwordLength"
        );
      }
      if (typeof expected.username !== "undefined") {
        is(login.username, expected.username, "Check username");
      }
      if (typeof expected.password !== "undefined") {
        is(login.password, expected.password, "Check password");
      }
      if (typeof expected.usedSince !== "undefined") {
        ok(login.timeLastUsed > expected.usedSince, "Check timeLastUsed");
      }
      if (typeof expected.passwordChangedSince !== "undefined") {
        ok(
          login.timePasswordChanged > expected.passwordChangedSince,
          "Check timePasswordChanged"
        );
      }
    }
  }
  return allLogins;
}

/**
 * Submit the content form and return a promise resolving to the username and
 * password values echoed out in the response
 *
 * @param {Object} [browser] - browser with the form
 * @param {String = ""} formAction - Optional url to set the form's action to before submitting
 * @param {Object = null} selectorValues - Optional object with field values to set before form submission
 * @param {Object = null} responseSelectors - Optional object with selectors to find the username and password in the response
 */
async function submitFormAndGetResults(
  browser,
  formAction = "",
  selectorValues,
  responseSelectors
) {
  function contentSubmitForm([contentFormAction, contentSelectorValues]) {
    let doc = content.document;
    let form = doc.querySelector("form");
    if (contentFormAction) {
      form.action = contentFormAction;
    }
    for (let [sel, value] of Object.entries(contentSelectorValues)) {
      try {
        doc.querySelector(sel).setUserInput(value);
      } catch (ex) {
        throw new Error(`submitForm: Couldn't set value of field at: ${sel}`);
      }
    }
    form.submit();
  }
  await ContentTask.spawn(
    browser,
    [formAction, selectorValues],
    contentSubmitForm
  );
  let result = await getFormSubmitResponseResult(
    browser,
    formAction,
    responseSelectors
  );
  return result;
}

/**
 * Wait for a given result page to load and return a promise resolving to an object with the parsed-out
 * username/password values from the response
 *
 * @param {Object} [browser] - browser which is loading this page
 * @param {String} resultURL - the path or filename to look for in the content.location
 * @param {Object = null} - Optional object with selectors to find the username and password in the response
 */
async function getFormSubmitResponseResult(
  browser,
  resultURL = "/formsubmit.sjs",
  { username = "#user", password = "#pass" } = {}
) {
  // default selectors are for the response page produced by formsubmit.sjs
  let fieldValues = await ContentTask.spawn(
    browser,
    {
      resultURL,
      usernameSelector: username,
      passwordSelector: password,
    },
    async function({ resultURL, usernameSelector, passwordSelector }) {
      await ContentTaskUtils.waitForCondition(() => {
        return (
          content.location.pathname.endsWith(resultURL) &&
          content.document.readyState == "complete"
        );
      }, `Wait for form submission load (${resultURL})`);
      let username = content.document.querySelector(usernameSelector)
        .textContent;
      let password = content.document.querySelector(passwordSelector)
        .textContent;
      return {
        username,
        password,
      };
    }
  );
  return fieldValues;
}

/**
 * Loads a test page in `DIRECTORY_URL` which automatically submits to formsubmit.sjs and returns a
 * promise resolving with the field values when the optional `aTaskFn` is done.
 *
 * @param {String} aPageFile - test page file name which auto-submits to formsubmit.sjs
 * @param {Function} aTaskFn - task which can be run before the tab closes.
 * @param {String} [aOrigin="http://example.com"] - origin of the server to use
 *                                                  to load `aPageFile`.
 */
function testSubmittingLoginForm(
  aPageFile,
  aTaskFn,
  aOrigin = "http://example.com"
) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: aOrigin + DIRECTORY_PATH + aPageFile,
    },
    async function(browser) {
      ok(true, "loaded " + aPageFile);
      let fieldValues = await getFormSubmitResponseResult(
        browser,
        "/formsubmit.sjs"
      );
      ok(true, "form submission loaded");
      if (aTaskFn) {
        await aTaskFn(fieldValues);
      }
      return fieldValues;
    }
  );
}

function checkOnlyLoginWasUsedTwice({ justChanged }) {
  // Check to make sure we updated the timestamps and use count on the
  // existing login that was submitted for the test.
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login");
  ok(logins[0] instanceof Ci.nsILoginMetaInfo, "metainfo QI");
  is(logins[0].timesUsed, 2, "check .timesUsed for existing login submission");
  ok(logins[0].timeCreated < logins[0].timeLastUsed, "timeLastUsed bumped");
  if (justChanged) {
    is(
      logins[0].timeLastUsed,
      logins[0].timePasswordChanged,
      "timeLastUsed == timePasswordChanged"
    );
  } else {
    is(
      logins[0].timeCreated,
      logins[0].timePasswordChanged,
      "timeChanged not updated"
    );
  }
}

function clearHttpAuths() {
  let authMgr = Cc["@mozilla.org/network/http-auth-manager;1"].getService(
    Ci.nsIHttpAuthManager
  );
  authMgr.clearAll();
}

// Begin popup notification (doorhanger) functions //

const REMEMBER_BUTTON = "button";
const NEVER_MENUITEM = 0;

const CHANGE_BUTTON = "button";
const DONT_CHANGE_BUTTON = "secondaryButton";

/**
 * Checks if we have a password capture popup notification
 * of the right type and with the right label.
 *
 * @param {String} aKind The desired `passwordNotificationType`
 * @param {Object} [popupNotifications = PopupNotifications]
 * @param {Object} [browser = null] Optional browser whose notifications should be searched.
 * @return the found password popup notification.
 */
function getCaptureDoorhanger(
  aKind,
  popupNotifications = PopupNotifications,
  browser = null
) {
  ok(true, "Looking for " + aKind + " popup notification");
  let notification = popupNotifications.getNotification("password", browser);
  if (notification) {
    is(
      notification.options.passwordNotificationType,
      aKind,
      "Notification type matches."
    );
    if (aKind == "password-change") {
      is(
        notification.mainAction.label,
        "Update",
        "Main action label matches update doorhanger."
      );
    } else if (aKind == "password-save") {
      is(
        notification.mainAction.label,
        "Save",
        "Main action label matches save doorhanger."
      );
    }
  }
  return notification;
}

async function getCaptureDoorhangerThatMayOpen(
  aKind,
  popupNotifications = PopupNotifications,
  browser = null
) {
  let notif = getCaptureDoorhanger(aKind, popupNotifications, browser);
  if (notif && !notif.dismissed) {
    if (popupNotifications.panel.state !== "open") {
      await BrowserTestUtils.waitForEvent(
        popupNotifications.panel,
        "popupshown"
      );
    }
  }
  return notif;
}

function getDoorhangerButton(aPopup, aButtonIndex) {
  let notifications = aPopup.owner.panel.children;
  ok(notifications.length > 0, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (aButtonIndex == "button") {
    return notification.button;
  } else if (aButtonIndex == "secondaryButton") {
    return notification.secondaryButton;
  }
  return notification.menupopup.querySelectorAll("menuitem")[aButtonIndex];
}

/**
 * Clicks the specified popup notification button.
 *
 * @param {Element} aPopup Popup Notification element
 * @param {Number} aButtonIndex Number indicating which button to click.
 *                              See the constants in this file.
 */
function clickDoorhangerButton(aPopup, aButtonIndex) {
  ok(true, "Looking for action at index " + aButtonIndex);

  let button = getDoorhangerButton(aPopup, aButtonIndex);
  if (aButtonIndex == "button") {
    ok(true, "Triggering main action");
  } else if (aButtonIndex == "secondaryButton") {
    ok(true, "Triggering secondary action");
  } else {
    ok(true, "Triggering menuitem # " + aButtonIndex);
  }
  button.doCommand();
}

async function cleanupDoorhanger(notif) {
  let PN = notif ? notif.owner : PopupNotifications;
  if (notif) {
    notif.remove();
  }
  let promiseHidden = PN.isPanelOpen
    ? BrowserTestUtils.waitForEvent(PN.panel, "popuphidden")
    : Promise.resolve();
  PN.panel.hidePopup();
  await promiseHidden;
}

/**
 * Checks the doorhanger's username and password.
 *
 * @param {String} username The username.
 * @param {String} password The password.
 */
async function checkDoorhangerUsernamePassword(username, password) {
  await BrowserTestUtils.waitForCondition(() => {
    return (
      document.getElementById("password-notification-username").value ==
      username
    );
  }, "Wait for nsLoginManagerPrompter writeDataToUI()");
  is(
    document.getElementById("password-notification-username").value,
    username,
    "Check doorhanger username"
  );
  is(
    document.getElementById("password-notification-password").value,
    password,
    "Check doorhanger password"
  );
}

/**
 * Change the doorhanger's username and password input values.
 *
 * @param {object} newValues
 *        named values to update
 * @param {string} [newValues.password = undefined]
 *        An optional string value to replace whatever is in the password field
 * @param {string} [newValues.username = undefined]
 *        An optional string value to replace whatever is in the username field
 */
async function updateDoorhangerInputValues(newValues) {
  let { panel } = PopupNotifications;
  is(panel.state, "open", "Check the doorhanger is already open");

  let notifElem = panel.childNodes[0];

  // Note: setUserInput does not reliably dispatch input events from chrome elements?
  async function setInputValue(target, value) {
    info(`setInputValue: on target: ${target.id}, value: ${value}`);
    target.focus();
    target.select();
    await EventUtils.synthesizeKey("KEY_Backspace");
    info(
      `setInputValue: target.value: ${target.value}, sending new value string`
    );
    await EventUtils.sendString(value);
    await EventUtils.synthesizeKey("KEY_Tab");
    return Promise.resolve();
  }

  let passwordField = notifElem.querySelector(
    "#password-notification-password"
  );
  let usernameField = notifElem.querySelector(
    "#password-notification-username"
  );

  if (typeof newValues.password !== "undefined") {
    if (passwordField.value !== newValues.password) {
      await setInputValue(passwordField, newValues.password);
    }
  }
  if (typeof newValues.username !== "undefined") {
    if (usernameField.value !== newValues.username) {
      await setInputValue(usernameField, newValues.username);
    }
  }
}

// End popup notification (doorhanger) functions //

async function waitForPasswordManagerDialog(openingFunc) {
  let win;
  await openingFunc();
  await TestUtils.waitForCondition(() => {
    win = Services.wm.getMostRecentWindow("Toolkit:PasswordManager");
    return win && win.document.getElementById("filter");
  }, "Waiting for the password manager dialog to open");

  return {
    filterValue: win.document.getElementById("filter").value,
    async close() {
      await BrowserTestUtils.closeWindow(win);
    },
  };
}

async function waitForPasswordManagerTab(openingFunc, waitForFilter) {
  info("waiting for new tab to open");
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null);
  await openingFunc();
  let tab = await tabPromise;
  ok(tab, "got password management tab");
  let filterValue;
  if (waitForFilter) {
    filterValue = await ContentTask.spawn(tab.linkedBrowser, null, async () => {
      let loginFilter = Cu.waiveXrays(
        content.document.querySelector("login-filter")
      );
      await ContentTaskUtils.waitForCondition(
        () => !!loginFilter.value,
        "wait for login-filter to have a value"
      );
      return loginFilter.value;
    });
  }
  return {
    filterValue,
    close() {
      BrowserTestUtils.removeTab(tab);
    },
  };
}

function openPasswordManager(openingFunc, waitForFilter) {
  return Services.prefs.getCharPref("signon.management.overrideURI")
    ? waitForPasswordManagerTab(openingFunc, waitForFilter)
    : waitForPasswordManagerDialog(openingFunc);
}

// Autocomplete popup related functions //

async function openACPopup(popup, browser, inputSelector) {
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
  return shown;
}

// Contextmenu functions //

/**
 * Synthesize mouse clicks to open the password manager context menu popup
 * for a target password input element.
 *
 * assertCallback should return true if we should continue or else false.
 */
async function openPasswordContextMenu(
  browser,
  passwordInput,
  assertCallback = null
) {
  const doc = browser.ownerDocument;
  const CONTEXT_MENU = doc.getElementById("contentAreaContextMenu");
  const POPUP_HEADER = doc.getElementById("fill-login");
  const LOGIN_POPUP = doc.getElementById("fill-login-popup");

  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    CONTEXT_MENU,
    "popupshown"
  );

  // Synthesize a right mouse click over the password input element, we have to trigger
  // both events because formfill code relies on this event happening before the contextmenu
  // (which it does for real user input) in order to not show the password autocomplete.
  let eventDetails = { type: "mousedown", button: 2 };
  await BrowserTestUtils.synthesizeMouseAtCenter(
    passwordInput,
    eventDetails,
    browser
  );
  // Synthesize a contextmenu event to actually open the context menu.
  eventDetails = { type: "contextmenu", button: 2 };
  await BrowserTestUtils.synthesizeMouseAtCenter(
    passwordInput,
    eventDetails,
    browser
  );

  await contextMenuShownPromise;

  if (assertCallback) {
    let shouldContinue = await assertCallback();
    if (!shouldContinue) {
      return;
    }
  }

  // Synthesize a mouse click over the fill login menu header.
  let popupShownPromise = BrowserTestUtils.waitForCondition(
    () => POPUP_HEADER.open && BrowserTestUtils.is_visible(LOGIN_POPUP)
  );
  EventUtils.synthesizeMouseAtCenter(POPUP_HEADER, {}, browser.ownerGlobal);
  await popupShownPromise;
}

/**
 * Use the contextmenu to fill a field with a generated password
 */
async function doFillGeneratedPasswordContextMenuItem(browser, passwordInput) {
  await SimpleTest.promiseFocus(browser);
  await openPasswordContextMenu(browser, passwordInput);

  let loginPopup = document.getElementById("fill-login-popup");
  let generatedPasswordItem = document.getElementById(
    "fill-login-generated-password"
  );
  let generatedPasswordSeparator = document.getElementById(
    "generated-password-separator"
  );

  // Check the content of the password manager popup
  ok(BrowserTestUtils.is_visible(loginPopup), "Popup is visible");
  ok(
    BrowserTestUtils.is_visible(generatedPasswordItem),
    "generated password item is visible"
  );
  ok(
    BrowserTestUtils.is_visible(generatedPasswordSeparator),
    "separator is visible"
  );

  let passwordChangedPromise = ContentTask.spawn(
    browser,
    [passwordInput],
    async function(passwordInput) {
      let input = content.document.querySelector(passwordInput);
      await ContentTaskUtils.waitForEvent(input, "input");
    }
  );
  let messagePromise = new Promise(resolve => {
    const eventName = "PasswordManager:onGeneratedPasswordFilledOrEdited";
    browser.messageManager.addMessageListener(eventName, function mgsHandler(
      msg
    ) {
      if (msg.target != browser) {
        return;
      }
      browser.messageManager.removeMessageListener(eventName, mgsHandler);
      info(
        "doFillGeneratedPasswordContextMenuItem: Got onGeneratedPasswordFilledOrEdited, resolving"
      );
      // allow LMP to handle the message, then resolve
      SimpleTest.executeSoon(resolve);
    });
  });

  EventUtils.synthesizeMouseAtCenter(generatedPasswordItem, {});
  info(
    "doFillGeneratedPasswordContextMenuItem: Waiting for content input event"
  );
  await passwordChangedPromise;
  await messagePromise;
}
