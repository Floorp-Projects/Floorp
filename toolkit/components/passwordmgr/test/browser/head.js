const DIRECTORY_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";

Cu.import("resource://testing-common/LoginTestUtils.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

registerCleanupFunction(function* cleanup_removeAllLoginsAndResetRecipes() {
  Services.logins.removeAllLogins();

  let recipeParent = LoginTestUtils.recipes.getRecipeParent();
  if (!recipeParent) {
    // No need to reset the recipes if the recipe module wasn't even loaded.
    return;
  }
  yield recipeParent.then(recipeParentResult => recipeParentResult.reset());
});

/**
 * Loads a test page in `DIRECTORY_URL` which automatically submits to formsubmit.sjs and returns a
 * promise resolving with the field values when the optional `aTaskFn` is done.
 *
 * @param {String} aPageFile - test page file name which auto-submits to formsubmit.sjs
 * @param {Function} aTaskFn - task which can be run before the tab closes.
 * @param {String} [aOrigin="http://example.com"] - origin of the server to use
 *                                                  to load `aPageFile`.
 */
function testSubmittingLoginForm(aPageFile, aTaskFn, aOrigin = "http://example.com") {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: aOrigin + DIRECTORY_PATH + aPageFile,
  }, function*(browser) {
    ok(true, "loaded " + aPageFile);
    let fieldValues = yield ContentTask.spawn(browser, undefined, function*() {
      yield ContentTaskUtils.waitForCondition(() => {
        return content.location.pathname.endsWith("/formsubmit.sjs") &&
          content.document.readyState == "complete";
      }, "Wait for form submission load (formsubmit.sjs)");
      let username = content.document.getElementById("user").textContent;
      let password = content.document.getElementById("pass").textContent;
      return {
        username,
        password,
      };
    });
    ok(true, "form submission loaded");
    if (aTaskFn) {
      yield* aTaskFn(fieldValues);
    }
    return fieldValues;
  });
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
    is(logins[0].timeLastUsed, logins[0].timePasswordChanged, "timeLastUsed == timePasswordChanged");
  } else {
    is(logins[0].timeCreated, logins[0].timePasswordChanged, "timeChanged not updated");
  }
}

// Begin popup notification (doorhanger) functions //

const REMEMBER_BUTTON = 0;
const NEVER_BUTTON = 1;

const CHANGE_BUTTON = 0;
const DONT_CHANGE_BUTTON = 1;

/**
 * Checks if we have a password capture popup notification
 * of the right type and with the right label.
 *
 * @param {String} aKind The desired `passwordNotificationType`
 * @param {Object} [popupNotifications = PopupNotifications]
 * @return the found password popup notification.
 */
function getCaptureDoorhanger(aKind, popupNotifications = PopupNotifications) {
  ok(true, "Looking for " + aKind + " popup notification");
  let notification = popupNotifications.getNotification("password");
  if (notification) {
    is(notification.options.passwordNotificationType, aKind, "Notification type matches.");
    if (aKind == "password-change") {
      is(notification.mainAction.label, "Update", "Main action label matches update doorhanger.");
    } else if (aKind == "password-save") {
      is(notification.mainAction.label, "Remember", "Main action label matches save doorhanger.");
    }
  }
  return notification;
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

  let notifications = aPopup.owner.panel.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (aButtonIndex == 0) {
    ok(true, "Triggering main action");
    notification.button.doCommand();
  } else if (aButtonIndex <= aPopup.secondaryActions.length) {
    ok(true, "Triggering secondary action " + aButtonIndex);
    notification.childNodes[aButtonIndex].doCommand();
  }
}

/**
 * Checks the doorhanger's username and password.
 *
 * @param {String} username The username.
 * @param {String} password The password.
 */
function* checkDoorhangerUsernamePassword(username, password) {
  yield BrowserTestUtils.waitForCondition(() => {
    return document.getElementById("password-notification-username").value == username;
  }, "Wait for nsLoginManagerPrompter writeDataToUI()");
  is(document.getElementById("password-notification-username").value, username,
     "Check doorhanger username");
  is(document.getElementById("password-notification-password").value, password,
     "Check doorhanger password");
}

// End popup notification (doorhanger) functions //
