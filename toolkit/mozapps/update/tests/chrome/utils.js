/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test Definition
 *
 * Most tests can use an array named TESTS that will perform most if not all of
 * the necessary checks. Each element in the array must be an object with the
 * following possible properties. Additional properties besides the ones listed
 * below can be added as needed.
 *
 * overrideCallback (optional)
 *   The function to call for the next test. This is typically called when the
 *   wizard page changes but can also be called for other events by the previous
 *   test. If this property isn't defined then the defaultCallback function will
 *   be called. If this property is defined then all other properties are
 *   optional.
 *
 * pageid (required unless overrideCallback is specified)
 *   The expected pageid for the wizard. This property is required unless the
 *   overrideCallback property is defined.
 *
 * extraStartFunction (optional)
 *   The function to call at the beginning of the defaultCallback function. If
 *   the function returns true the defaultCallback function will return early
 *   which allows waiting for a specific condition to be evaluated in the
 *   function specified in the extraStartFunction property before continuing
 *   with the test.
 *
 * extraCheckFunction (optional)
 *   The function to call to perform extra checks in the defaultCallback
 *   function.
 *
 * extraDelayedCheckFunction (optional)
 *   The function to call to perform extra checks in the delayedDefaultCallback
 *   function.
 *
 * buttonStates (optional)
 *   A javascript object representing the expected hidden and disabled attribute
 *   values for the buttons of the current wizard page. The values are checked
 *   in the delayedDefaultCallback function. For information about the structure
 *   of this object refer to the getExpectedButtonStates and checkButtonStates
 *   functions.
 *
 * buttonClick (optional)
 *   The current wizard page button to click at the end of the
 *   delayedDefaultCallback function. If the buttonClick property is defined
 *   then the extraDelayedFinishFunction property can't be specified due to race
 *   conditions in some of the tests and if both of them are specified the test
 *   will intentionally throw.
 *
 * extraDelayedFinishFunction (optional)
 *   The function to call at the end of the delayedDefaultCallback function.
 *   If the extraDelayedFinishFunction property is defined then the buttonClick
 *   property can't be specified due to race conditions in some of the tests and
 *   if both of them are specified the test will intentionally throw.
 *
 * ranTest (should not be specified)
 *   When delayedDefaultCallback is called a property named ranTest is added to
 *   the current test so it is possible to verify that each test in the TESTS
 *   array has ran.
 *
 * prefHasUserValue (optional)
 *   For comparing the expected value defined by this property with the return
 *   value of prefHasUserValue using gPrefToCheck for the preference name in the
 *   checkPrefHasUserValue function.
 */

'use strict';

/* globals TESTS, runTest, finishTest */

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr,
        utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm", this);

const IS_MACOSX = ("nsILocalFileMac" in Ci);
const IS_WIN = ("@mozilla.org/windows-registry-key;1" in Cc);

// The tests have to use the pageid instead of the pageIndex due to the
// app update wizard's access method being random.
const PAGEID_DUMMY            = "dummy";                 // Done
const PAGEID_CHECKING         = "checking";              // Done
const PAGEID_NO_UPDATES_FOUND = "noupdatesfound";        // Done
const PAGEID_MANUAL_UPDATE    = "manualUpdate";          // Done
const PAGEID_UNSUPPORTED      = "unsupported";           // Done
const PAGEID_FOUND_BASIC      = "updatesfoundbasic";     // Done
const PAGEID_DOWNLOADING      = "downloading";           // Done
const PAGEID_ERRORS           = "errors";                // Done
const PAGEID_ERROR_EXTRA      = "errorextra";            // Done
const PAGEID_ERROR_PATCHING   = "errorpatching";         // Done
const PAGEID_FINISHED         = "finished";              // Done
const PAGEID_FINISHED_BKGRD   = "finishedBackground";    // Done

const UPDATE_WINDOW_NAME = "Update:Wizard";

const URL_HOST = "http://example.com";
const URL_PATH_UPDATE_XML = "/chrome/toolkit/mozapps/update/tests/chrome/update.sjs";
const REL_PATH_DATA = "chrome/toolkit/mozapps/update/tests/data";

// These two URLs must not contain parameters since tests add their own
// test specific parameters.
const URL_HTTP_UPDATE_XML = URL_HOST + URL_PATH_UPDATE_XML;
const URL_HTTPS_UPDATE_XML = "https://example.com" + URL_PATH_UPDATE_XML;

const URI_UPDATE_PROMPT_DIALOG  = "chrome://mozapps/content/update/updates.xul";

const PREF_APP_UPDATE_INTERVAL = "app.update.interval";
const PREF_APP_UPDATE_LASTUPDATETIME = "app.update.lastUpdateTime.background-update-timer";

const LOG_FUNCTION = info;

const BIN_SUFFIX = (IS_WIN ? ".exe" : "");
const FILE_UPDATER_BIN = "updater" + (IS_MACOSX ? ".app" : BIN_SUFFIX);
const FILE_UPDATER_BIN_BAK = FILE_UPDATER_BIN + ".bak";

var gURLData = URL_HOST + "/" + REL_PATH_DATA + "/";

var gTestTimeout = 240000; // 4 minutes
var gTimeoutTimer;

// The number of SimpleTest.executeSoon calls to perform when waiting on an
// update window to close before giving up.
const CLOSE_WINDOW_TIMEOUT_MAXCOUNT = 10;
// Counter for the SimpleTest.executeSoon when waiting on an update window to
// close before giving up.
var gCloseWindowTimeoutCounter = 0;

// The following vars are for restoring previous preference values (if present)
// when the test finishes.
var gAppUpdateEnabled;            // app.update.enabled
var gAppUpdateServiceEnabled;     // app.update.service.enabled
var gAppUpdateStagingEnabled;     // app.update.staging.enabled
var gAppUpdateURLDefault;         // app.update.url (default prefbranch)

var gTestCounter = -1;
var gWin;
var gDocElem;
var gPrefToCheck;
var gUseTestUpdater = false;

// Set to true to log additional information for debugging. To log additional
// information for an individual test set DEBUG_AUS_TEST to true in the test's
// onload function.
var DEBUG_AUS_TEST = true;

const DATA_URI_SPEC = "chrome://mochitests/content/chrome/toolkit/mozapps/update/tests/data/";
/* import-globals-from ../data/shared.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "shared.js", this);

/**
 * The current test in TESTS array.
 */
this.__defineGetter__("gTest", function() {
  return TESTS[gTestCounter];
});

/**
 * The current test's callback. This will either return the callback defined in
 * the test's overrideCallback property or defaultCallback if the
 * overrideCallback property is undefined.
 */
this.__defineGetter__("gCallback", function() {
  return gTest.overrideCallback ? gTest.overrideCallback
                                : defaultCallback;
});

/**
 * nsIObserver for receiving window open and close notifications.
 */
const gWindowObserver = {
  observe: function WO_observe(aSubject, aTopic, aData) {
    let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);

    if (aTopic == "domwindowclosed") {
      if (win.location != URI_UPDATE_PROMPT_DIALOG) {
        debugDump("domwindowclosed event for window not being tested - " +
                  "location: " + win.location + "... returning early");
        return;
      }
      // Allow tests the ability to provide their own function (it must be
      // named finishTest) for finishing the test.
      try {
        finishTest();
      }
      catch (e) {
        finishTestDefault();
      }
      return;
    }

    win.addEventListener("load", function WO_observe_onLoad() {
      win.removeEventListener("load", WO_observe_onLoad, false);
      // Ignore windows other than the update UI window.
      if (win.location != URI_UPDATE_PROMPT_DIALOG) {
        debugDump("load event for window not being tested - location: " +
                  win.location + "... returning early");
        return;
      }

      // The first wizard page should always be the dummy page.
      let pageid = win.document.documentElement.currentPage.pageid;
      if (pageid != PAGEID_DUMMY) {
        // This should never happen but if it does this will provide a clue
        // for diagnosing the cause.
        ok(false, "Unexpected load event - pageid got: " + pageid +
           ", expected: " + PAGEID_DUMMY + "... returning early");
        return;
      }

      gWin = win;
      gDocElem = gWin.document.documentElement;
      gDocElem.addEventListener("pageshow", onPageShowDefault, false);
    }, false);
  }
};

/**
 * Default test run function that can be used by most tests. This function uses
 * protective measures to prevent the test from failing provided by
 * |runTestDefaultWaitForWindowClosed| helper functions to prevent failure due
 * to a previous test failure.
 */
function runTestDefault() {
  debugDump("entering");

  if (!("@mozilla.org/zipwriter;1" in Cc)) {
    ok(false, "nsIZipWriter is required to run these tests");
    return;
  }

  SimpleTest.waitForExplicitFinish();

  runTestDefaultWaitForWindowClosed();
}

/**
 * If an update window is found SimpleTest.executeSoon can callback before the
 * update window is fully closed especially with debug builds. If an update
 * window is found this function will call itself using SimpleTest.executeSoon
 * up to the amount declared in CLOSE_WINDOW_TIMEOUT_MAXCOUNT until the update
 * window has closed before continuing the test.
 */
function runTestDefaultWaitForWindowClosed() {
  gCloseWindowTimeoutCounter++;
  if (gCloseWindowTimeoutCounter > CLOSE_WINDOW_TIMEOUT_MAXCOUNT) {
    try {
      finishTest();
    }
    catch (e) {
      finishTestDefault();
    }
    return;
  }

  // The update window should not be open at this time. If it is the call to
  // |closeUpdateWindow| will close it and cause the test to fail.
  if (closeUpdateWindow()) {
    SimpleTest.executeSoon(runTestDefaultWaitForWindowClosed);
  } else {
    Services.ww.registerNotification(gWindowObserver);

    gCloseWindowTimeoutCounter = 0;

    setupFiles();
    setupPrefs();
    gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "1");
    removeUpdateDirsAndFiles();
    reloadUpdateManagerData();
    setupTimer(gTestTimeout);
    SimpleTest.executeSoon(setupTestUpdater);
  }
}

/**
 * Default test finish function that can be used by most tests. This function
 * uses protective measures to prevent the next test from failing provided by
 * |finishTestDefaultWaitForWindowClosed| helper functions to prevent failure
 * due to an update window being left open.
 */
function finishTestDefault() {
  debugDump("entering");
  if (gTimeoutTimer) {
    gTimeoutTimer.cancel();
    gTimeoutTimer = null;
  }

  if (gChannel) {
    debugDump("channel = " + gChannel);
    gChannel = null;
    gPrefRoot.removeObserver(PREF_APP_UPDATE_CHANNEL, observer);
  }

  verifyTestsRan();

  resetPrefs();
  gEnv.set("MOZ_TEST_SKIP_UPDATE_STAGE", "");
  resetFiles();
  removeUpdateDirsAndFiles();
  reloadUpdateManagerData();

  Services.ww.unregisterNotification(gWindowObserver);
  if (gDocElem) {
    gDocElem.removeEventListener("pageshow", onPageShowDefault, false);
  }

  finishTestRestoreUpdaterBackup();
}

/**
 * nsITimerCallback for the timeout timer to cleanly finish a test if the Update
 * Window doesn't close for a test. This allows the next test to run properly if
 * a previous test fails.
 *
 * @param  aTimer
 *         The nsITimer that fired.
 */
function finishTestTimeout(aTimer) {
  ok(false, "Test timed out. Maximum time allowed is " + (gTestTimeout / 1000) +
     " seconds");

  try {
    finishTest();
  }
  catch (e) {
    finishTestDefault();
  }
}

/**
 * When a test finishes this will repeatedly attempt to restore the real updater
 * for tests that use the test updater and then call
 * finishTestDefaultWaitForWindowClosed after the restore is successful.
 */
function finishTestRestoreUpdaterBackup() {
  if (gUseTestUpdater) {
    try {
      // Windows debug builds keep the updater file in use for a short period of
      // time after the updater process exits.
      restoreUpdaterBackup();
    } catch (e) {
      logTestInfo("Attempt to restore the backed up updater failed... " +
                  "will try again, Exception: " + e);
      SimpleTest.executeSoon(finishTestRestoreUpdaterBackup);
      return;
    }
  }

  finishTestDefaultWaitForWindowClosed();
}

/**
 * If an update window is found SimpleTest.executeSoon can callback before the
 * update window is fully closed especially with debug builds. If an update
 * window is found this function will call itself using SimpleTest.executeSoon
 * up to the amount declared in CLOSE_WINDOW_TIMEOUT_MAXCOUNT until the update
 * window has closed before finishing the test.
 */
function finishTestDefaultWaitForWindowClosed() {
  gCloseWindowTimeoutCounter++;
  if (gCloseWindowTimeoutCounter > CLOSE_WINDOW_TIMEOUT_MAXCOUNT) {
    SimpleTest.requestCompleteLog();
    SimpleTest.finish();
    return;
  }

  // The update window should not be open at this time. If it is the call to
  // |closeUpdateWindow| will close it and cause the test to fail.
  if (closeUpdateWindow()) {
    SimpleTest.executeSoon(finishTestDefaultWaitForWindowClosed);
  } else {
    SimpleTest.finish();
  }
}

/**
 * Default callback for the wizard's documentElement pageshow listener. This
 * will return early for event's where the originalTarget's nodeName is not
 * wizardpage.
 */
function onPageShowDefault(aEvent) {
  if (!gTimeoutTimer) {
    debugDump("gTimeoutTimer is null... returning early");
    return;
  }

  // Return early if the event's original target isn't for a wizardpage element.
  // This check is necessary due to the remotecontent element firing pageshow.
  if (aEvent.originalTarget.nodeName != "wizardpage") {
    debugDump("only handles events with an originalTarget nodeName of " +
              "|wizardpage|. aEvent.originalTarget.nodeName = " +
              aEvent.originalTarget.nodeName + "... returning early");
    return;
  }

  gTestCounter++;
  gCallback(aEvent);
}

/**
 * Default callback that can be used by most tests.
 */
function defaultCallback(aEvent) {
  if (!gTimeoutTimer) {
    debugDump("gTimeoutTimer is null... returning early");
    return;
  }

  debugDump("entering - TESTS[" + gTestCounter + "], pageid: " + gTest.pageid +
            ", aEvent.originalTarget.nodeName: " +
            aEvent.originalTarget.nodeName);

  if (gTest && gTest.extraStartFunction) {
    debugDump("calling extraStartFunction " + gTest.extraStartFunction.name);
    if (gTest.extraStartFunction(aEvent)) {
      debugDump("extraStartFunction early return");
      return;
    }
  }

  is(gDocElem.currentPage.pageid, gTest.pageid,
     "Checking currentPage.pageid equals " + gTest.pageid + " in pageshow");

  // Perform extra checks if specified by the test
  if (gTest.extraCheckFunction) {
    debugDump("calling extraCheckFunction " + gTest.extraCheckFunction.name);
    gTest.extraCheckFunction();
  }

  // The wizard page buttons' disabled and hidden attributes are set after the
  // pageshow event so use executeSoon to allow them to be set so their disabled
  // and hidden attribute values can be checked.
  SimpleTest.executeSoon(delayedDefaultCallback);
}

/**
 * Delayed default callback called using executeSoon in defaultCallback which
 * allows the wizard page buttons' disabled and hidden attributes to be set
 * before checking their values.
 */
function delayedDefaultCallback() {
  if (!gTimeoutTimer) {
    debugDump("gTimeoutTimer is null... returning early");
    return;
  }

  if (!gTest) {
    debugDump("gTest is null... returning early");
    return;
  }

  debugDump("entering - TESTS[" + gTestCounter + "], pageid: " + gTest.pageid);

  // Verify the pageid hasn't changed after executeSoon was called.
  is(gDocElem.currentPage.pageid, gTest.pageid,
     "Checking currentPage.pageid equals " + gTest.pageid + " after " +
     "executeSoon");

  checkButtonStates();

  // Perform delayed extra checks if specified by the test
  if (gTest.extraDelayedCheckFunction) {
    debugDump("calling extraDelayedCheckFunction " +
              gTest.extraDelayedCheckFunction.name);
    gTest.extraDelayedCheckFunction();
  }

  // Used to verify that this test has been performed
  gTest.ranTest = true;

  if (gTest.buttonClick) {
    debugDump("clicking " + gTest.buttonClick + " button");
    if (gTest.extraDelayedFinishFunction) {
      throw ("Tests cannot have a buttonClick and an extraDelayedFinishFunction property");
    }
    gDocElem.getButton(gTest.buttonClick).click();
  } else if (gTest.extraDelayedFinishFunction) {
    debugDump("calling extraDelayedFinishFunction " +
              gTest.extraDelayedFinishFunction.name);
    gTest.extraDelayedFinishFunction();
  }
}

/**
 * Gets the continue file used to signal the mock http server to continue
 * downloading for slow download mar file tests without creating it.
 *
 * @return nsILocalFile for the continue file.
 */
function getContinueFile() {
  let continueFile = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties).
                     get("CurWorkD", Ci.nsILocalFile);
  let continuePath = REL_PATH_DATA + "/continue";
  let continuePathParts = continuePath.split("/");
  for (let i = 0; i < continuePathParts.length; ++i) {
    continueFile.append(continuePathParts[i]);
  }
  return continueFile;
}

/**
 * Creates the continue file used to signal the mock http server to continue
 * downloading for slow download mar file tests.
 */
function createContinueFile() {
  debugDump("creating 'continue' file for slow mar downloads");
  writeFile(getContinueFile(), "");
}

/**
 * Removes the continue file used to signal the mock http server to continue
 * downloading for slow download mar file tests.
 */
function removeContinueFile() {
  let continueFile = getContinueFile();
  if (continueFile.exists()) {
    debugDump("removing 'continue' file for slow mar downloads");
    continueFile.remove(false);
  }
}

/**
 * Checks the wizard page buttons' disabled and hidden attributes values are
 * correct. If an expected button id is not specified then the expected disabled
 * and hidden attribute value is true.
 */
function checkButtonStates() {
  debugDump("entering - TESTS[" + gTestCounter + "], pageid: " + gTest.pageid);

  const buttonNames = ["extra1", "extra2", "back", "next", "finish", "cancel"];
  let buttonStates = getExpectedButtonStates();
  buttonNames.forEach(function(aButtonName) {
    let button = gDocElem.getButton(aButtonName);
    let hasHidden = aButtonName in buttonStates &&
                    "hidden" in buttonStates[aButtonName];
    let hidden = hasHidden ? buttonStates[aButtonName].hidden : true;
    let hasDisabled = aButtonName in buttonStates &&
                      "disabled" in buttonStates[aButtonName];
    let disabled = hasDisabled ? buttonStates[aButtonName].disabled : true;
    is(button.hidden, hidden, "Checking " + aButtonName + " button " +
       "hidden attribute value equals " + (hidden ? "true" : "false"));
    is(button.disabled, disabled, "Checking " + aButtonName + " button " +
       "disabled attribute value equals " + (disabled ? "true" : "false"));
  });
}

/**
 * Returns the expected disabled and hidden attribute values for the buttons of
 * the current wizard page.
 */
function getExpectedButtonStates() {
  // Allow individual tests to override the expected button states.
  if (gTest.buttonStates) {
    return gTest.buttonStates;
  }

  switch (gTest.pageid) {
    case PAGEID_CHECKING:
      return {cancel: {disabled: false, hidden: false}};
    case PAGEID_FOUND_BASIC:
      if (gTest.neverButton) {
        return {extra1: {disabled: false, hidden: false},
                extra2: {disabled: false, hidden: false},
                next: {disabled: false, hidden: false}};
      }
      return {extra1: {disabled: false, hidden: false},
              next: {disabled: false, hidden: false}};
    case PAGEID_DOWNLOADING:
      return {extra1: {disabled: false, hidden: false}};
    case PAGEID_NO_UPDATES_FOUND:
    case PAGEID_MANUAL_UPDATE:
    case PAGEID_UNSUPPORTED:
    case PAGEID_ERRORS:
    case PAGEID_ERROR_EXTRA:
      return {finish: {disabled: false, hidden: false}};
    case PAGEID_ERROR_PATCHING:
      return {next: { disabled: false, hidden: false}};
    case PAGEID_FINISHED:
    case PAGEID_FINISHED_BKGRD:
      return {extra1: { disabled: false, hidden: false},
              finish: { disabled: false, hidden: false}};
  }
  return null;
}

/**
 * Compares the return value of prefHasUserValue for the preference specified in
 * gPrefToCheck with the value passed in the aPrefHasValue parameter or the
 * value specified in the current test's prefHasUserValue property if
 * aPrefHasValue is undefined.
 *
 * @param  aPrefHasValue (optional)
 *         The expected value returned from prefHasUserValue for the preference
 *         specified in gPrefToCheck. If aPrefHasValue is undefined the value
 *         of the current test's prefHasUserValue property will be used.
 */
function checkPrefHasUserValue(aPrefHasValue) {
  let prefHasUserValue = aPrefHasValue === undefined ? gTest.prefHasUserValue
                                                     : aPrefHasValue;
  is(Services.prefs.prefHasUserValue(gPrefToCheck), prefHasUserValue,
     "Checking prefHasUserValue for preference " + gPrefToCheck + " equals " +
     (prefHasUserValue ? "true" : "false"));
}

/**
 * Checks whether the link is hidden for a general background update check error
 * or not on the errorextra page and that the app.update.backgroundErrors
 * preference does not have a user value.
 *
 * @param  aShouldBeHidden (optional)
 *         The expected value for the label's hidden attribute for the link. If
 *         aShouldBeHidden is undefined the value of the current test's
 *         shouldBeHidden property will be used.
 */
function checkErrorExtraPage(aShouldBeHidden) {
  let shouldBeHidden = aShouldBeHidden === undefined ? gTest.shouldBeHidden
                                                     : aShouldBeHidden;
  is(gWin.document.getElementById("errorExtraLinkLabel").hidden, shouldBeHidden,
     "Checking errorExtraLinkLabel hidden attribute equals " +
     (shouldBeHidden ? "true" : "false"));

  is(gWin.document.getElementById(gTest.displayedTextElem).hidden, false,
     "Checking " + gTest.displayedTextElem + " should not be hidden");

  ok(!Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS),
     "Preference " + PREF_APP_UPDATE_BACKGROUNDERRORS + " should not have a " +
     "user value");
}

/**
 * Gets the update version info for the update url parameters to send to
 * update.sjs.
 *
 * @param  aAppVersion (optional)
 *         The application version for the update snippet. If not specified the
 *         current application version will be used.
 * @return The url parameters for the application and platform version to send
 *         to update.sjs.
 */
function getVersionParams(aAppVersion) {
  let appInfo = Services.appinfo;
  return "&appVersion=" + (aAppVersion ? aAppVersion : appInfo.version);
}

/**
 * Verifies that all tests ran.
 */
function verifyTestsRan() {
  debugDump("entering");

  // Return early if there are no tests defined.
  if (!TESTS) {
    return;
  }

  gTestCounter = -1;
  for (let i = 0; i < TESTS.length; ++i) {
    gTestCounter++;
    let test = TESTS[i];
    let msg = "Checking if TESTS[" + i + "] test was performed... " +
              "callback function name = " + gCallback.name + ", " +
              "pageid = " + test.pageid;
    ok(test.ranTest, msg);
  }
}

/**
 * Creates a backup of files the tests need to modify so they can be restored to
 * the original file when the test has finished and then modifies the files.
 */
function setupFiles() {
  // Backup the updater-settings.ini file if it exists by moving it.
  let baseAppDir = getGREDir();
  let updateSettingsIni = baseAppDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  if (updateSettingsIni.exists()) {
    updateSettingsIni.moveTo(baseAppDir, FILE_UPDATE_SETTINGS_INI_BAK);
  }
  updateSettingsIni = baseAppDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);
}

/**
 * For tests that use the test updater restores the backed up real updater if
 * it exists and tries again on failure since Windows debug builds at times
 * leave the file in use. After success moveRealUpdater is called to continue
 * the setup of the test updater. For tests that don't use the test updater
 * runTest will be called.
 */
function setupTestUpdater() {
  if (!gUseTestUpdater) {
    runTest();
    return;
  }

  try {
    restoreUpdaterBackup();
  } catch (e) {
    logTestInfo("Attempt to restore the backed up updater failed... " +
                "will try again, Exception: " + e);
    SimpleTest.executeSoon(setupTestUpdater);
    return;
  }
  moveRealUpdater();
}

/**
 * Backs up the real updater and tries again on failure since Windows debug
 * builds at times leave the file in use. After success it will call
 * copyTestUpdater to continue the setup of the test updater.
 */
function moveRealUpdater() {
  try {
    // Move away the real updater
    let baseAppDir = getAppBaseDir();
    let updater = baseAppDir.clone();
    updater.append(FILE_UPDATER_BIN);
    updater.moveTo(baseAppDir, FILE_UPDATER_BIN_BAK);
  } catch (e) {
    logTestInfo("Attempt to move the real updater out of the way failed... " +
                "will try again, Exception: " + e);
    SimpleTest.executeSoon(moveRealUpdater);
    return;
  }

  copyTestUpdater();
}

/**
 * Copies the test updater so it can be used by tests and tries again on failure
 * since Windows debug builds at times leave the file in use. After success it
 * will call runTest to continue the test.
 */
function copyTestUpdater() {
  try {
    // Copy the test updater
    let baseAppDir = getAppBaseDir();
    let testUpdaterDir = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
    let relPath = REL_PATH_DATA;
    let pathParts = relPath.split("/");
    for (let i = 0; i < pathParts.length; ++i) {
      testUpdaterDir.append(pathParts[i]);
    }

    let testUpdater = testUpdaterDir.clone();
    testUpdater.append(FILE_UPDATER_BIN);
    testUpdater.copyToFollowingLinks(baseAppDir, FILE_UPDATER_BIN);
  } catch (e) {
    logTestInfo("Attempt to copy the test updater failed... " +
                "will try again, Exception: " + e);
    SimpleTest.executeSoon(copyTestUpdater);
    return;
  }

  runTest();
}

/**
 * Restores the updater that was backed up. This is called in setupTestUpdater
 * before the backup of the real updater is done in case the previous test
 * failed to restore the updater, in finishTestDefaultWaitForWindowClosed when
 * the test has finished, and in test_9999_cleanup.xul after all tests have
 * finished.
 */
function restoreUpdaterBackup() {
  let baseAppDir = getAppBaseDir();
  let updater = baseAppDir.clone();
  let updaterBackup = baseAppDir.clone();
  updater.append(FILE_UPDATER_BIN);
  updaterBackup.append(FILE_UPDATER_BIN_BAK);
  if (updaterBackup.exists()) {
    if (updater.exists()) {
      updater.remove(true);
    }
    updaterBackup.moveTo(baseAppDir, FILE_UPDATER_BIN);
  }
}

/**
 * Sets the most common preferences used by tests to values used by the majority
 * of the tests and when necessary saves the preference's original values if
 * present so they can be set back to the original values when the test has
 * finished.
 */
function setupPrefs() {
  if (DEBUG_AUS_TEST) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, true);
  }

  // Prevent nsIUpdateTimerManager from notifying nsIApplicationUpdateService
  // to check for updates by setting the app update last update time to the
  // current time minus one minute in seconds and the interval time to 12 hours
  // in seconds.
  let now = Math.round(Date.now() / 1000) - 60;
  Services.prefs.setIntPref(PREF_APP_UPDATE_LASTUPDATETIME, now);
  Services.prefs.setIntPref(PREF_APP_UPDATE_INTERVAL, 43200);

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ENABLED)) {
    gAppUpdateEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_ENABLED);
  }
  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, true);

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SERVICE_ENABLED)) {
    gAppUpdateServiceEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED);
  }
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, false);

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_STAGING_ENABLED)) {
    gAppUpdateStagingEnabled = Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED);
  }
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);

  Services.prefs.setIntPref(PREF_APP_UPDATE_IDLETIME, 0);
  Services.prefs.setIntPref(PREF_APP_UPDATE_PROMPTWAITTIME, 0);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);
}

/**
 * Restores files that were backed up for the tests and general file cleanup.
 */
function resetFiles() {
  // Restore the backed up updater-settings.ini if it exists.
  let baseAppDir = getGREDir();
  let updateSettingsIni = baseAppDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI_BAK);
  if (updateSettingsIni.exists()) {
    updateSettingsIni.moveTo(baseAppDir, FILE_UPDATE_SETTINGS_INI);
  }

  // Not being able to remove the "updated" directory will not adversely affect
  // subsequent tests so wrap it in a try block and don't test whether its
  // removal was successful.
  let updatedDir;
  if (IS_MACOSX) {
    updatedDir = getUpdatesDir();
    updatedDir.append(DIR_PATCH);
  } else {
    updatedDir = getAppBaseDir();
  }
  updatedDir.append(DIR_UPDATED);
  if (updatedDir.exists()) {
    try {
      removeDirRecursive(updatedDir);
    }
    catch (e) {
      logTestInfo("Unable to remove directory. Path: " + updatedDir.path +
                  ", Exception: " + e);
    }
  }
}

/**
 * Resets the most common preferences used by tests to their original values.
 */
function resetPrefs() {
  if (gAppUpdateURLDefault) {
    gDefaultPrefBranch.setCharPref(PREF_APP_UPDATE_URL, gAppUpdateURLDefault);
  }

  if (gAppUpdateEnabled !== undefined) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, gAppUpdateEnabled);
  } else if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ENABLED)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_ENABLED);
  }

  if (gAppUpdateServiceEnabled !== undefined) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, gAppUpdateServiceEnabled);
  } else if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SERVICE_ENABLED)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ENABLED);
  }

  if (gAppUpdateStagingEnabled !== undefined) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, gAppUpdateStagingEnabled);
  } else if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_STAGING_ENABLED)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_STAGING_ENABLED);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_IDLETIME)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_IDLETIME);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_PROMPTWAITTIME)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_PROMPTWAITTIME);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_URL_DETAILS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_URL_DETAILS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_LOG)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_LOG);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SILENT)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SILENT);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDMAXERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDMAXERRORS);
  }

  try {
    Services.prefs.deleteBranch(PREFBRANCH_APP_UPDATE_NEVER);
  }
  catch (e) {
  }
}

function setupTimer(aTestTimeout) {
  gTestTimeout = aTestTimeout;
  if (gTimeoutTimer) {
    gTimeoutTimer.cancel();
    gTimeoutTimer = null;
  }
  gTimeoutTimer = Cc["@mozilla.org/timer;1"].
                  createInstance(Ci.nsITimer);
  gTimeoutTimer.initWithCallback(finishTestTimeout, gTestTimeout,
                                 Ci.nsITimer.TYPE_ONE_SHOT);
}

/**
 * Closes the update window if it is open and causes the test to fail if an
 * update window is found.
 *
 * @return true if an update window was found, otherwise false.
 */
function closeUpdateWindow() {
  let updateWindow = getUpdateWindow();
  if (!updateWindow) {
    return false;
  }

  ok(false, "Found an existing Update Window from the current or a previous " +
            "test... attempting to close it.");
  updateWindow.close();
  return true;
}

/**
 * Gets the update window.
 *
 * @return The nsIDOMWindow for the Update Window if it is open and null
 *         if it isn't.
 */
function getUpdateWindow() {
  return Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME);
}

/**
 * Helper for background check errors.
 */
const errorsPrefObserver = {
  observedPref: null,
  maxErrorPref: null,

  /**
   * Sets up a preference observer and sets the associated maximum errors
   * preference used for background notification.
   *
   * @param  aObservePref
   *         The preference to observe.
   * @param  aMaxErrorPref
   *         The maximum errors preference.
   * @param  aMaxErrorCount
   *         The value to set the maximum errors preference to.
   */
  init: function(aObservePref, aMaxErrorPref, aMaxErrorCount) {
    this.observedPref = aObservePref;
    this.maxErrorPref = aMaxErrorPref;

    let maxErrors = aMaxErrorCount ? aMaxErrorCount : 2;
    Services.prefs.setIntPref(aMaxErrorPref, maxErrors);
    Services.prefs.addObserver(aObservePref, this, false);
  },

  /**
   * Preference observer for the preference specified in |this.observedPref|.
   */
  observe: function XPI_observe(aSubject, aTopic, aData) {
    if (aData == this.observedPref) {
      let errCount = Services.prefs.getIntPref(this.observedPref);
      let errMax = Services.prefs.getIntPref(this.maxErrorPref);
      if (errCount >= errMax) {
        debugDump("removing pref observer");
        Services.prefs.removeObserver(this.observedPref, this);
      } else {
        debugDump("notifying AUS");
        SimpleTest.executeSoon(function() {
          gAUS.notify(null);
        });
      }
    }
  }
};
