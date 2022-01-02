const DIRECTORY_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";

const { LoginHelper } = ChromeUtils.import(
  "resource://gre/modules/LoginHelper.jsm"
);
const { LoginManagerParent } = ChromeUtils.import(
  "resource://gre/modules/LoginManagerParent.jsm"
);
const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

add_task(async function common_initialize() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.rememberSignons", true],
      ["signon.testOnlyUserHasInteractedByPrefValue", true],
      ["signon.testOnlyUserHasInteractedWithDocument", true],
      ["toolkit.telemetry.ipcBatchTimeout", 0],
    ],
  });
  if (LoginHelper.relatedRealmsEnabled) {
    await LoginTestUtils.remoteSettings.setupWebsitesWithSharedCredentials();
    registerCleanupFunction(async function() {
      await LoginTestUtils.remoteSettings.cleanWebsitesWithSharedCredentials();
    });
  }
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
    await cleanupPasswordNotifications();
    await closePopup(document.getElementById("contentAreaContextMenu"));
    await closePopup(document.getElementById("PopupAutoComplete"));
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
      if (typeof expected.timeCreated !== "undefined") {
        is(login.timeCreated, expected.timeCreated, "Check timeCreated");
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
  async function contentSubmitForm([contentFormAction, contentSelectorValues]) {
    const { WrapPrivileged } = ChromeUtils.import(
      "resource://specialpowers/WrapPrivileged.jsm"
    );
    let doc = content.document;
    let form = doc.querySelector("form");
    if (contentFormAction) {
      form.action = contentFormAction;
    }
    for (let [sel, value] of Object.entries(contentSelectorValues)) {
      try {
        let field = doc.querySelector(sel);
        let gotInput = ContentTaskUtils.waitForEvent(
          field,
          "input",
          "Got input event on " + sel
        );
        // we don't get an input event if the new value == the old
        field.value = "###";
        WrapPrivileged.wrap(field, this).setUserInput(value);
        await gotInput;
      } catch (ex) {
        throw new Error(
          `submitForm: Couldn't set value of field at: ${sel}: ${ex.message}`
        );
      }
    }
    form.submit();
  }

  let loadPromise = BrowserTestUtils.browserLoaded(browser);
  await SpecialPowers.spawn(
    browser,
    [[formAction, selectorValues]],
    contentSubmitForm
  );
  await loadPromise;

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
      // Bug 1686071: Since generated passwords can have special characters in them,
      // we need to unescape the characters. These special characters are automatically escaped
      // when we submit a form in `submitFormAndGetResults`.
      // Otherwise certain tests will intermittently fail when these special characters are present in the passwords.
      let password = unescape(
        content.document.querySelector(passwordSelector).textContent
      );
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
        await aTaskFn(fieldValues, browser);
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
const REMOVE_LOGIN_MENUITEM = 0;

/**
 * Checks if we have a password capture popup notification
 * of the right type and with the right label.
 *
 * @param {String} aKind The desired `passwordNotificationType` ("any" for any type)
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
  if (!aKind) {
    throw new Error(
      "getCaptureDoorhanger needs aKind to be a non-empty string"
    );
  }
  if (aKind !== "any" && notification) {
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

async function waitForDoorhanger(browser, type) {
  let notif;
  await TestUtils.waitForCondition(() => {
    notif = PopupNotifications.getNotification("password", browser);
    if (notif && type !== "any") {
      return (
        notif.options.passwordNotificationType == type &&
        notif.anchorElement &&
        BrowserTestUtils.is_visible(notif.anchorElement)
      );
    }
    return notif;
  }, `Waiting for a ${type} notification`);
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

function getDoorhangerButton(aPopup, aButtonIndex) {
  let notifications = aPopup.owner.panel.children;
  ok(!!notifications.length, "at least one notification displayed");
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

async function cleanupPasswordNotifications(
  popupNotifications = PopupNotifications
) {
  let notif;
  while ((notif = popupNotifications.getNotification("password"))) {
    notif.remove();
  }
}

async function clearMessageCache(browser) {
  await SpecialPowers.spawn(browser, [], async () => {
    const { LoginManagerChild } = ChromeUtils.import(
      "resource://gre/modules/LoginManagerChild.jsm"
    );
    let docState = LoginManagerChild.forWindow(content).stateForDocument(
      content.document
    );
    docState.lastSubmittedValuesByRootElement = new content.WeakMap();
  });
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
        username &&
      document.getElementById("password-notification-password").value ==
        password
    );
  }, "Wait for nsLoginManagerPrompter writeDataToUI() to update to the correct username/password values");
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
 * @param {Object} [popupNotifications = PopupNotifications]
 */
async function updateDoorhangerInputValues(
  newValues,
  popupNotifications = PopupNotifications
) {
  let { panel } = popupNotifications;
  if (popupNotifications.panel.state !== "open") {
    await BrowserTestUtils.waitForEvent(popupNotifications.panel, "popupshown");
  }
  is(panel.state, "open", "Check the doorhanger is already open");

  let notifElem = panel.childNodes[0];

  // Note: setUserInput does not reliably dispatch input events from chrome elements?
  async function setInputValue(target, value) {
    info(`setInputValue: on target: ${target.id}, value: ${value}`);
    target.focus();
    target.select();
    info(
      `setInputValue: current value: '${target.value}', setting new value '${value}'`
    );
    await EventUtils.synthesizeKey("KEY_Backspace");
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

/**
 * Open doorhanger autocomplete popup and select a username value.
 *
 * @param {string} text the text value of the username that should be selected.
 *                 Noop if `text` is falsy.
 */
async function selectDoorhangerUsername(text) {
  await _selectDoorhanger(
    text,
    "#password-notification-username",
    "#password-notification-username-dropmarker"
  );
}

/**
 * Open doorhanger autocomplete popup and select a password value.
 *
 * @param {string} text the text value of the password that should be selected.
 *                 Noop if `text` is falsy.
 */
async function selectDoorhangerPassword(text) {
  await _selectDoorhanger(
    text,
    "#password-notification-password",
    "#password-notification-password-dropmarker"
  );
}

async function _selectDoorhanger(text, inputSelector, dropmarkerSelector) {
  if (!text) {
    return;
  }

  info("Opening doorhanger suggestion popup");

  let doorhangerPopup = document.getElementById("password-notification");
  let dropmarker = doorhangerPopup.querySelector(dropmarkerSelector);

  let autocompletePopup = document.getElementById("PopupAutoComplete");
  let popupShown = BrowserTestUtils.waitForEvent(
    autocompletePopup,
    "popupshown"
  );
  // the dropmarker gets un-hidden async when looking up username suggestions
  await TestUtils.waitForCondition(() => !dropmarker.hidden);

  EventUtils.synthesizeMouseAtCenter(dropmarker, {});

  await popupShown;

  let suggestions = [
    ...document
      .getElementById("PopupAutoComplete")
      .getElementsByTagName("richlistitem"),
  ].filter(richlistitem => !richlistitem.collapsed);

  let suggestionText = suggestions.map(
    richlistitem => richlistitem.querySelector(".ac-title-text").innerHTML
  );

  let targetIndex = suggestionText.indexOf(text);
  ok(targetIndex != -1, "Suggestions include expected text");

  let promiseHidden = BrowserTestUtils.waitForEvent(
    autocompletePopup,
    "popuphidden"
  );

  info("Selecting doorhanger suggestion");

  EventUtils.synthesizeMouseAtCenter(suggestions[targetIndex], {});

  await promiseHidden;
}

// End popup notification (doorhanger) functions //

async function openPasswordManager(openingFunc, waitForFilter) {
  info("waiting for new tab to open");
  let tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    url => url.includes("about:logins") && !url.includes("entryPoint="),
    true
  );
  await openingFunc();
  let tab = await tabPromise;
  ok(tab, "got password management tab");
  let filterValue;
  if (waitForFilter) {
    filterValue = await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
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

// Autocomplete popup related functions //

async function openACPopup(
  popup,
  browser,
  inputSelector,
  iframeBrowsingContext = null
) {
  let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

  await SimpleTest.promiseFocus(browser);
  info("content window focused");

  // Focus the username field to open the popup.
  let target = iframeBrowsingContext || browser;
  await SpecialPowers.spawn(
    target,
    [[inputSelector]],
    function openAutocomplete(sel) {
      content.document.querySelector(sel).focus();
    }
  );

  let shown = await promiseShown;
  ok(shown, "autocomplete popup shown");
  return shown;
}

async function closePopup(popup) {
  if (popup.state == "closed") {
    await Promise.resolve();
  } else {
    let promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
    popup.hidePopup();
    await promiseHidden;
  }
}

async function fillGeneratedPasswordFromOpenACPopup(
  browser,
  passwordInputSelector
) {
  let popup = browser.ownerDocument.getElementById("PopupAutoComplete");
  let item;

  await new Promise(requestAnimationFrame);
  await TestUtils.waitForCondition(() => {
    item = popup.querySelector(`[originaltype="generatedPassword"]`);
    return item && !EventUtils.isHidden(item);
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

  let passwordGeneratedPromise = listenForTestNotification(
    "PasswordEditedOrGenerated"
  );

  info("Clicking the generated password AC item");
  EventUtils.synthesizeMouseAtCenter(item, {});
  info("Waiting for the content input value to change");
  await inputEventPromise;
  info("Waiting for the passwordGeneratedPromise");
  await passwordGeneratedPromise;
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
  input,
  assertCallback = null,
  browsingContext = null,
  openFillMenu = null
) {
  const doc = browser.ownerDocument;
  const CONTEXT_MENU = doc.getElementById("contentAreaContextMenu");
  const POPUP_HEADER = doc.getElementById("fill-login");
  const LOGIN_POPUP = doc.getElementById("fill-login-popup");

  if (!browsingContext) {
    browsingContext = browser.browsingContext;
  }

  let contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    CONTEXT_MENU,
    "popupshown"
  );

  // Synthesize a right mouse click over the password input element, we have to trigger
  // both events because formfill code relies on this event happening before the contextmenu
  // (which it does for real user input) in order to not show the password autocomplete.
  let eventDetails = { type: "mousedown", button: 2 };
  await BrowserTestUtils.synthesizeMouseAtCenter(
    input,
    eventDetails,
    browsingContext
  );
  // Synthesize a contextmenu event to actually open the context menu.
  eventDetails = { type: "contextmenu", button: 2 };
  await BrowserTestUtils.synthesizeMouseAtCenter(
    input,
    eventDetails,
    browsingContext
  );

  await contextMenuShownPromise;

  if (assertCallback) {
    let shouldContinue = await assertCallback();
    if (!shouldContinue) {
      return;
    }
  }

  if (openFillMenu) {
    // Open the fill login menu.
    let popupShownPromise = BrowserTestUtils.waitForEvent(
      LOGIN_POPUP,
      "popupshown"
    );
    POPUP_HEADER.openMenu(true);
    await popupShownPromise;
  }
}

/**
 * Listen for the login manager test notification specified by
 * expectedMessage. Possible messages:
 *   FormProcessed - a form was processed after page load.
 *   FormSubmit - a form was just submitted.
 *   PasswordEditedOrGenerated - a password was filled in or modified.
 *
 * The count is the number of that messages to wait for. This should
 * typically be used when waiting for the FormProcessed message for a page
 * that has subframes to ensure all have been handled.
 *
 * Returns a promise that will passed additional data specific to the message.
 */
function listenForTestNotification(expectedMessage, count = 1) {
  return new Promise(resolve => {
    LoginManagerParent.setListenerForTests((msg, data) => {
      if (msg == expectedMessage && --count == 0) {
        LoginManagerParent.setListenerForTests(null);
        info("listenForTestNotification, resolving for message: " + msg);
        resolve(data);
      }
    });
  });
}

/**
 * Use the contextmenu to fill a field with a generated password
 */
async function doFillGeneratedPasswordContextMenuItem(browser, passwordInput) {
  await SimpleTest.promiseFocus(browser);
  await openPasswordContextMenu(browser, passwordInput);

  let generatedPasswordItem = document.getElementById(
    "fill-login-generated-password"
  );
  let generatedPasswordSeparator = document.getElementById(
    "passwordmgr-items-separator"
  );

  ok(
    BrowserTestUtils.is_visible(generatedPasswordItem),
    "generated password item is visible"
  );
  ok(
    BrowserTestUtils.is_visible(generatedPasswordSeparator),
    "separator is visible"
  );

  let popup = document.getElementById("PopupAutoComplete");
  ok(popup, "Got popup");
  let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

  await new Promise(resolve => {
    SimpleTest.executeSoon(resolve);
  });

  let contextMenu = document.getElementById("contentAreaContextMenu");
  contextMenu.activateItem(generatedPasswordItem);

  await promiseShown;
  await fillGeneratedPasswordFromOpenACPopup(browser, passwordInput);
}

// Content form helpers
async function changeContentFormValues(
  browser,
  selectorValues,
  shouldBlur = true
) {
  for (let [sel, value] of Object.entries(selectorValues)) {
    info("changeContentFormValues, update: " + sel + ", to: " + value);
    await changeContentInputValue(browser, sel, value, shouldBlur);
    await TestUtils.waitForTick();
  }
}

async function changeContentInputValue(
  browser,
  selector,
  str,
  shouldBlur = true
) {
  await SimpleTest.promiseFocus(browser.ownerGlobal);
  let oldValue = await ContentTask.spawn(browser, [selector], function(sel) {
    return content.document.querySelector(sel).value;
  });

  if (str === oldValue) {
    info("no change needed to value of " + selector + ": " + oldValue);
    return;
  }
  info(`changeContentInputValue: from "${oldValue}" to "${str}"`);
  await ContentTask.spawn(
    browser,
    { selector, str, shouldBlur },
    async function({ selector, str, shouldBlur }) {
      const EventUtils = ContentTaskUtils.getEventUtils(content);
      let input = content.document.querySelector(selector);

      input.focus();
      if (!str) {
        input.select();
        await EventUtils.synthesizeKey("KEY_Backspace", {}, content);
      } else if (input.value.startsWith(str)) {
        info(
          `New string is substring of value: ${str.length}, ${input.value.length}`
        );
        input.setSelectionRange(str.length, input.value.length);
        await EventUtils.synthesizeKey("KEY_Backspace", {}, content);
      } else if (str.startsWith(input.value)) {
        info(
          `New string appends to value: ${input.value}, ${str.substring(
            input.value.length
          )}`
        );
        input.setSelectionRange(input.value.length, input.value.length);
        await EventUtils.sendString(str.substring(input.value.length), content);
      } else {
        input.select();
        await EventUtils.sendString(str, content);
      }

      if (shouldBlur) {
        let changedPromise = ContentTaskUtils.waitForEvent(input, "change");
        input.blur();
        await changedPromise;
      }

      is(str, input.value, `Expected value '${str}' is set on input`);
    }
  );
  info("Input value changed");
  await TestUtils.waitForTick();
}

async function verifyConfirmationHint(
  browser,
  forceClose,
  anchorID = "password-notification-icon"
) {
  let hintElem = browser.ownerGlobal.ConfirmationHint._panel;
  await BrowserTestUtils.waitForPopupEvent(hintElem, "shown");
  try {
    is(hintElem.state, "open", "hint popup is open");
    ok(
      BrowserTestUtils.is_visible(hintElem.anchorNode),
      "hint anchorNode is visible"
    );
    is(
      hintElem.anchorNode.id,
      anchorID,
      "Hint should be anchored on the expected notification icon"
    );
    info("verifyConfirmationHint, hint is shown and has its anchorNode");
    if (forceClose) {
      await closePopup(hintElem);
    } else {
      info("verifyConfirmationHint, assertion ok, wait for poopuphidden");
      await BrowserTestUtils.waitForPopupEvent(hintElem, "hidden");
      info("verifyConfirmationHint, hintElem popup is hidden");
    }
  } catch (ex) {
    ok(false, "Confirmation hint not shown: " + ex.message);
  } finally {
    info("verifyConfirmationHint promise finalized");
  }
}
