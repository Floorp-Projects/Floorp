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
 *   test. If this property isn't defined then the defailtCallback function will
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
 *
 * expectedRadioGroupSelectedIndex (optional)
 *   For comparing the expected selectedIndex attribute value of the wizard's
 *   license page radiogroup selectedIndex attribute in the
 *   checkRadioGroupSelectedIndex function.
 *
 * expectedRemoteContentState (optional)
 *   For comparing the expected remotecontent state attribute value of the
 *   wizard's billboard and license pages in the checkRemoteContentState and
 *   waitForRemoteContentLoaded functions.
 *
 *
 * Test Add-ons
 *
 * All tests include the test add-ons specified in the TEST_ADDONS array and
 * the only thing that can be configured is whether the noupdate test add-on is
 * disabled (see below). The add-on names are in the format of typename_X where
 * X is a number to make the add-on ID unique and typename is one of the values
 * specified below:
 *
 * appdisabled
 *   disabled by the application due to being incompatible with the current
 *   toolkit version.
 *
 * compatible
 *   compatible with the current toolkit version and the update's toolkit
 *   version.
 *
 * noupdate
 *   the add-on is compatible with the current toolkit version and does not have
 *   an update to make it compatible with the update's toolkit version. Tests
 *   that need to have all add-ons compatible for the application update can
 *   disable this add-on by setting the gDisableNoUpdateAddon variable to true.
 *
 * updatecompatibility
 *   the add-on is compatible with the current toolkit version and has a
 *   compatibility update to make it compatible with the update's toolkit
 *   version. Tests that need to have add-ons compatible for the application
 *   update without updating the add-on's compatibility information can disable
 *   this add-on by setting the gDisableUpdateCompatibilityAddon variable to
 *   true.
 *
 * updateversion
 *   the add-on is compatible with the current toolkit version and has a version
 *   update to make it compatible with the update's toolkit version. Tests that
 *   need to have add-ons compatible for the application update without updating
 *   the add-on's to a new version that is compatible can disable this add-on by
 *   setting the gDisableUpdateVersionAddon variable to true.
 *
 * userdisabled
 *   disabled by the user and compatible with the current toolkit version but
 *   not the update's toolkit version. This add-on will be disabled after its
 *   install completes.
 */

'use strict';

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr,
        utils: Cu } = Components;

Cu.import("resource://gre/modules/AddonManager.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

const IS_MACOSX = ("nsILocalFileMac" in Ci);
const IS_WIN = ("@mozilla.org/windows-registry-key;1" in Cc);

// The tests have to use the pageid instead of the pageIndex due to the
// app update wizard's access method being random.
const PAGEID_DUMMY            = "dummy";                 // Done
const PAGEID_CHECKING         = "checking";              // Done
const PAGEID_PLUGIN_UPDATES   = "pluginupdatesfound";
const PAGEID_NO_UPDATES_FOUND = "noupdatesfound";        // Done
const PAGEID_MANUAL_UPDATE    = "manualUpdate"; // Tested on license load failure
const PAGEID_UNSUPPORTED      = "unsupported";           // Done
const PAGEID_INCOMPAT_CHECK   = "incompatibleCheck";     // Done
const PAGEID_FOUND_BASIC      = "updatesfoundbasic";     // Done
const PAGEID_FOUND_BILLBOARD  = "updatesfoundbillboard"; // Done
const PAGEID_LICENSE          = "license";               // Done
const PAGEID_INCOMPAT_LIST    = "incompatibleList";      // Done
const PAGEID_DOWNLOADING      = "downloading";           // Done
const PAGEID_ERRORS           = "errors";                // Done
const PAGEID_ERROR_EXTRA      = "errorextra";            // Done
const PAGEID_ERROR_PATCHING   = "errorpatching";         // Done
const PAGEID_FINISHED         = "finished";              // Done
const PAGEID_FINISHED_BKGRD   = "finishedBackground";    // Done
const PAGEID_INSTALLED        = "installed";             // Done

const UPDATE_WINDOW_NAME = "Update:Wizard";

const URL_HOST = "http://example.com";
const URL_PATH_UPDATE_XML = "/chrome/toolkit/mozapps/update/tests/chrome/update.sjs";
const REL_PATH_DATA = "chrome/toolkit/mozapps/update/tests/data";

// These two URLs must not contain parameters since tests add their own
// test specific parameters.
const URL_HTTP_UPDATE_XML = URL_HOST + URL_PATH_UPDATE_XML;
const URL_HTTPS_UPDATE_XML = "https://example.com" + URL_PATH_UPDATE_XML;

const URI_UPDATE_PROMPT_DIALOG  = "chrome://mozapps/content/update/updates.xul";

const ADDON_ID_SUFFIX = "@appupdatetest.mozilla.org";
const ADDON_PREP_DIR = "appupdateprep";

const PREF_APP_UPDATE_INTERVAL = "app.update.interval";
const PREF_APP_UPDATE_LASTUPDATETIME = "app.update.lastUpdateTime.background-update-timer";

// Preference for storing add-ons that are disabled by the tests to prevent them
// from interefering with the tests.
const PREF_DISABLEDADDONS = "app.update.test.disabledAddons";
const PREF_EM_HOTFIX_ID = "extensions.hotfix.id";
const TEST_ADDONS = [ "appdisabled_1", "appdisabled_2",
                      "compatible_1", "compatible_2",
                      "noupdate_1", "noupdate_2",
                      "updatecompatibility_1", "updatecompatibility_2",
                      "updateversion_1", "updateversion_2",
                      "userdisabled_1", "userdisabled_2", "hotfix" ];

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
var gAppUpdateURL;                // app.update.url.override
var gExtUpdateURL;                // extensions.update.url

var gTestCounter = -1;
var gWin;
var gDocElem;
var gPrefToCheck;
var gDisableNoUpdateAddon = false;
var gDisableUpdateCompatibilityAddon = false;
var gDisableUpdateVersionAddon = false;

// Set to true to log additional information for debugging. To log additional
// information for an individual test set DEBUG_AUS_TEST to true in the test's
// onload function.
var DEBUG_AUS_TEST = false;

const DATA_URI_SPEC = "chrome://mochitests/content/chrome/toolkit/mozapps/update/tests/data/";
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
 * The remotecontent element for the current page if one exists or null if a
 * remotecontent element doesn't exist.
 */
this.__defineGetter__("gRemoteContent", function() {
  switch (gTest.pageid) {
    case PAGEID_FOUND_BILLBOARD:
      return gWin.document.getElementById("updateMoreInfoContent");
    case PAGEID_LICENSE:
      return gWin.document.getElementById("licenseContent");
  }
  return null;
});

/**
 * The state for the remotecontent element if one exists or null if a
 * remotecontent element doesn't exist.
 */
this.__defineGetter__("gRemoteContentState", function() {
  if (gRemoteContent) {
    return gRemoteContent.getAttribute("state");
  }
  return null;
});

/**
 * The radiogroup for the license page.
 */
this.__defineGetter__("gAcceptDeclineLicense", function() {
  return gWin.document.getElementById("acceptDeclineLicense");
});

/**
 * The listbox for the incompatibleList page.
 */
this.__defineGetter__("gIncompatibleListbox", function() {
  return gWin.document.getElementById("incompatibleListbox");
});

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
    removeUpdateDirsAndFiles();
    reloadUpdateManagerData();
    setupAddons(runTest);
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
  resetFiles();
  removeUpdateDirsAndFiles();
  reloadUpdateManagerData();

  Services.ww.unregisterNotification(gWindowObserver);
  if (gDocElem) {
    gDocElem.removeEventListener("pageshow", onPageShowDefault, false);
  }

  finishTestDefaultWaitForWindowClosed();
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
 * If an update window is found SimpleTest.executeSoon can callback before the
 * update window is fully closed especially with debug builds. If an update
 * window is found this function will call itself using SimpleTest.executeSoon
 * up to the amount declared in CLOSE_WINDOW_TIMEOUT_MAXCOUNT until the update
 * window has closed before finishing the test.
 */
function finishTestDefaultWaitForWindowClosed() {
  gCloseWindowTimeoutCounter++;
  if (gCloseWindowTimeoutCounter > CLOSE_WINDOW_TIMEOUT_MAXCOUNT) {
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
      throw("Tests cannot have a buttonClick and an extraDelayedFinishFunction property");
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
    case PAGEID_INCOMPAT_CHECK:
      return { cancel: { disabled: false, hidden: false } };
    case PAGEID_FOUND_BASIC:
    case PAGEID_FOUND_BILLBOARD:
      if (gTest.neverButton) {
        return { extra1: { disabled: false, hidden: false },
                 extra2: { disabled: false, hidden: false },
                 next  : { disabled: false, hidden: false } }
      }
      return { extra1: { disabled: false, hidden: false },
               next  : { disabled: false, hidden: false } };
    case PAGEID_LICENSE:
      if (gRemoteContentState != "loaded" ||
          gAcceptDeclineLicense.selectedIndex != 0) {
        return { extra1: { disabled: false, hidden: false },
                 next  : { disabled: true, hidden: false } };
      }
      return { extra1: { disabled: false, hidden: false },
               next  : { disabled: false, hidden: false } };
    case PAGEID_INCOMPAT_LIST:
      return { extra1: { disabled: false, hidden: false },
               next  : { disabled: false, hidden: false } };
    case PAGEID_DOWNLOADING:
      return { extra1: { disabled: false, hidden: false } };
    case PAGEID_NO_UPDATES_FOUND:
    case PAGEID_MANUAL_UPDATE:
    case PAGEID_UNSUPPORTED:
    case PAGEID_ERRORS:
    case PAGEID_ERROR_EXTRA:
    case PAGEID_INSTALLED:
      return { finish: { disabled: false, hidden: false } };
    case PAGEID_ERROR_PATCHING:
      return { next  : { disabled: false, hidden: false } };
    case PAGEID_FINISHED:
    case PAGEID_FINISHED_BKGRD:
      return { extra1: { disabled: false, hidden: false },
               finish: { disabled: false, hidden: false } };
  }
  return null;
}

/**
 * Adds a load event listener to the current remotecontent element.
 */
function addRemoteContentLoadListener() {
  debugDump("entering - TESTS[" + gTestCounter + "], pageid: " + gTest.pageid);

  gRemoteContent.addEventListener("load", remoteContentLoadListener, false);
}

/**
 * The nsIDOMEventListener for a remotecontent load event.
 */
function remoteContentLoadListener(aEvent) {
  // Return early if the event's original target's nodeName isn't remotecontent.
  if (aEvent.originalTarget.nodeName != "remotecontent") {
    debugDump("only handles events with an originalTarget nodeName of " +
              "|remotecontent|. aEvent.originalTarget.nodeName = " +
              aEvent.originalTarget.nodeName);
    return;
  }

  gTestCounter++;
  gCallback(aEvent);
}

/**
 * Waits until a remotecontent element to finish loading which is determined
 * by the current test's expectedRemoteContentState property and then removes
 * the event listener.
 *
 * Note: tests that use this function should not test the state of the
 *      remotecontent since this will check the expected state.
 *
 * @return false if the remotecontent has loaded and its state is the state
 *         specified in the current test's expectedRemoteContentState
 *         property... otherwise true.
 */
function waitForRemoteContentLoaded(aEvent) {
  // Return early until the remotecontent has loaded with the state that is
  // expected or isn't the event's originalTarget.
  if (gRemoteContentState != gTest.expectedRemoteContentState ||
      aEvent.originalTarget != gRemoteContent) {
    debugDump("returning early. " +
              "gRemoteContentState: " +
              gRemoteContentState + ", " +
              "expectedRemoteContentState: " +
              gTest.expectedRemoteContentState + ", " +
              "aEvent.originalTarget.nodeName: " +
              aEvent.originalTarget.nodeName);
    return true;
  }

  gRemoteContent.removeEventListener("load", remoteContentLoadListener, false);
  return false;
}

/**
 * Compares the value of the remotecontent state attribute with the value
 * specified in the test's expectedRemoteContentState property.
 */
function checkRemoteContentState() {
  is(gRemoteContentState, gTest.expectedRemoteContentState, "Checking remote " +
     "content state equals " + gTest.expectedRemoteContentState + " - pageid " +
     gTest.pageid);
}

/**
 * Adds a select event listener to the license radiogroup element and clicks
 * the radio element specified in the current test's radioClick property.
 */
function addRadioGroupSelectListenerAndClick() {
  debugDump("entering - TESTS[" + gTestCounter + "], pageid: " + gTest.pageid);

  gAcceptDeclineLicense.addEventListener("select", radioGroupSelectListener,
                                         false);
  gWin.document.getElementById(gTest.radioClick).click();
}

/**
 * The nsIDOMEventListener for the license radiogroup select event.
 */
function radioGroupSelectListener(aEvent) {
  // Return early if the event's original target's nodeName isn't radiogroup.
  if (aEvent.originalTarget.nodeName != "radiogroup") {
    debugDump("only handles events with an originalTarget nodeName of " +
              "|radiogroup|. aEvent.originalTarget.nodeName = " +
              aEvent.originalTarget.nodeName);
    return;
  }

  gAcceptDeclineLicense.removeEventListener("select", radioGroupSelectListener,
                                            false);
  gTestCounter++;
  gCallback(aEvent);
}

/**
 * Compares the value of the License radiogroup's selectedIndex attribute with
 * the value specified in the test's expectedRadioGroupSelectedIndex property.
 */
function checkRadioGroupSelectedIndex() {
  is(gAcceptDeclineLicense.selectedIndex, gTest.expectedRadioGroupSelectedIndex,
     "Checking license radiogroup selectedIndex equals " +
     gTest.expectedRadioGroupSelectedIndex);
}

/**
 * Checks that only incompatible add-ons (e.g. noupdate_X add-ons) that don't
 * have an update are listed in the add-ons incompatible list.
 */
function checkIncompatbleList() {
  for (let i = 0; i < gIncompatibleListbox.itemCount; i++) {
    let label = gIncompatibleListbox.getItemAtIndex(i).label;
    // Use indexOf since locales can change the text displayed
    ok(label.indexOf("noupdate") != -1, "Checking that only incompatible " + 
       "add-ons that don't have an update are listed in the incompatible list");
  }
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
 * Checks whether the link is hidden (general background update check error or
 * a certificate attribute check error with an update) or not (certificate
 * attribute check error without an update) on the errorextra page and that the
 * app.update.cert.errors and app.update.backgroundErrors preferences do not
 & have a user value.
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

  ok(!Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_ERRORS),
     "Preference " + PREF_APP_UPDATE_CERT_ERRORS + " should not have a " +
     "user value");

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
 * @param  aPlatformVersion (optional)
 *         The platform version for the update snippet. If not specified the
 *         current platform version will be used.
 * @return The url parameters for the application and platform version to send
 *         to update.sjs.
 */
function getVersionParams(aAppVersion, aPlatformVersion) {
  let appInfo = Services.appinfo;
  return "&appVersion=" + (aAppVersion ? aAppVersion : appInfo.version) +
         "&platformVersion=" + (aPlatformVersion ? aPlatformVersion
                                                 : appInfo.platformVersion);
}

/**
 * Gets an application version that is greater than the current application
 * version. The version is created by taking the first sequence from the current
 * application version and adding 1 to it.
 *
 * @return A version string greater than the current application version string.
 */
function getNewerAppVersion() {
  let appVersion = Services.appinfo.version.split(".")[0];
  appVersion++;
  return appVersion;
}

/**
 * Gets a platform version that is greater than the current platform version.
 * The version is created by taking the first sequence from the current platform
 * version and adding 1 to it.
 *
 * @return A version string greater than the current platform version string.
 */
function getNewerPlatformVersion() {
  let platformVersion = Services.appinfo.platformVersion.split(".")[0];
  platformVersion++;
  return platformVersion;
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
 * Restore the updater that was backed up.  This is called both in setupFiles
 * and resetFiles.  It is called in setupFiles before the backup is done in
 * case the previous test failed.  It is called in resetFiles to put things
 * back to its original state.
 */
function resetUpdaterBackup() {
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
 * Creates a backup of files the tests need to modify so they can be restored to
 * the original file when the test has finished and then modifies the files.
 */
function setupFiles() {
  // Backup the updater-settings.ini file if it exists by moving it.
  let baseAppDir = getAppBaseDir();
  let updateSettingsIni = baseAppDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  if (updateSettingsIni.exists()) {
    updateSettingsIni.moveTo(baseAppDir, FILE_UPDATE_SETTINGS_INI_BAK);
  }
  updateSettingsIni = baseAppDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  // Just in case the last test failed, try to reset.
  resetUpdaterBackup();

  // Move away the real updater
  let updater = baseAppDir.clone();
  updater.append(FILE_UPDATER_BIN);
  updater.moveTo(baseAppDir, FILE_UPDATER_BIN_BAK);

  // Move in the test only updater
  let testUpdaterDir = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties).
    get("CurWorkD", Ci.nsILocalFile);

  let relPath = REL_PATH_DATA;
  let pathParts = relPath.split("/");
  for (let i = 0; i < pathParts.length; ++i) {
    testUpdaterDir.append(pathParts[i]);
  }

  let testUpdater = testUpdaterDir.clone();
  testUpdater.append(FILE_UPDATER_BIN);
  if (testUpdater.exists()) {
    testUpdater.copyToFollowingLinks(baseAppDir, FILE_UPDATER_BIN);
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

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_URL_OVERRIDE)) {
    gAppUpdateURL = Services.prefs.getCharPref(PREF_APP_UPDATE_URL_OVERRIDE);
  }

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

  if (Services.prefs.prefHasUserValue(PREF_EXTENSIONS_UPDATE_URL)) {
    gExtUpdateURL = Services.prefs.getCharPref(PREF_EXTENSIONS_UPDATE_URL);
  }
  let extUpdateUrl = URL_HTTP_UPDATE_XML + "?addonID=%ITEM_ID%" +
                     "&platformVersion=" + Services.appinfo.platformVersion +
                     "&newerPlatformVersion=" + getNewerPlatformVersion();
  Services.prefs.setCharPref(PREF_EXTENSIONS_UPDATE_URL, extUpdateUrl);

  Services.prefs.setIntPref(PREF_APP_UPDATE_IDLETIME, 0);
  Services.prefs.setIntPref(PREF_APP_UPDATE_PROMPTWAITTIME, 0);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);
  Services.prefs.setBoolPref(PREF_EXTENSIONS_STRICT_COMPAT, true);
  Services.prefs.setCharPref(PREF_EM_HOTFIX_ID, "hotfix" + ADDON_ID_SUFFIX);
}

/**
 * Restores files that were backed up for the tests and general file cleanup.
 */
function resetFiles() {
  // Restore the backed up updater-settings.ini if it exists.
  let baseAppDir = getAppBaseDir();
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
  resetUpdaterBackup();
}

/**
 * Resets the most common preferences used by tests to their original values.
 */
function resetPrefs() {
  if (gAppUpdateURL !== undefined) {
    Services.prefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, gAppUpdateURL);
  } else if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_URL_OVERRIDE)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_URL_OVERRIDE);
  }

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

  if (gExtUpdateURL !== undefined) {
    Services.prefs.setCharPref(PREF_EXTENSIONS_UPDATE_URL, gExtUpdateURL);
  } else if (Services.prefs.prefHasUserValue(PREF_EXTENSIONS_UPDATE_URL)) {
    Services.prefs.clearUserPref(PREF_EXTENSIONS_UPDATE_URL);
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

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SHOW_INSTALLED_UI)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SHOW_INSTALLED_UI);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_LOG)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_LOG);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_ERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_ERRORS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_MAXERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_MAXERRORS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDMAXERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDMAXERRORS);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_INVALID_ATTR_NAME)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_INVALID_ATTR_NAME);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_REQUIREBUILTIN)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_REQUIREBUILTIN);
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CERT_CHECKATTRS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CERT_CHECKATTRS);
  }

  try {
    CERT_ATTRS.forEach(function(aCertAttrName) {
      Services.prefs.clearUserPref(PREF_APP_UPDATE_CERTS_BRANCH + "1." +
                                   aCertAttrName);
    });
  }
  catch (e) {
  }

  try {
    Services.prefs.deleteBranch(PREF_APP_UPDATE_NEVER_BRANCH);
  }
  catch(e) {
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SILENT)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SILENT);
  }

  if (Services.prefs.prefHasUserValue(PREF_EXTENSIONS_STRICT_COMPAT)) {
		Services.prefs.clearUserPref(PREF_EXTENSIONS_STRICT_COMPAT);
  }

  if (Services.prefs.prefHasUserValue(PREF_EM_HOTFIX_ID)) {
    Services.prefs.clearUserPref(PREF_EM_HOTFIX_ID);
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
 * Disables pre-existing add-ons so they don't interfere with the tests,
 * installs the test add-ons, sets the noupdate test add-ons' userDisabled value
 * for the test, and calls the callback specified in the aCallback parameter. If
 * the app.update.test.disabledAddons has a user value then setting the noupdate
 * test add-ons' userDisabled value for the test is the only thing that is done.
 *
 * @param  aCallback
 *         A callback to call after all operations have completed.
 */
function setupAddons(aCallback) {
  debugDump("entering");

  // Sets the appropriate userDisabled value for the noupdate test add-ons based
  // on the value of gDisableNoUpdateAddon and calls the callback specified in
  // setupAddons aCallback parameter.
  function setNoUpdateAddonsDisabledState() {
    AddonManager.getAllAddons(function(aAddons) {
      aAddons.forEach(function(aAddon) {
        if (aAddon.name.startsWith("appdisabled")) {
          if (!aAddon.userDisabled) {
            aAddon.userDisabled = true;
          }
        }

        if (aAddon.name.startsWith("noupdate")) {
          if (aAddon.userDisabled != gDisableNoUpdateAddon) {
            aAddon.userDisabled = gDisableNoUpdateAddon;
          }
        }

        if (aAddon.name.startsWith("updatecompatibility")) {
          if (aAddon.userDisabled != gDisableUpdateCompatibilityAddon) {
            aAddon.userDisabled = gDisableUpdateCompatibilityAddon;
          }
        }

        if (aAddon.name.startsWith("updateversion")) {
          if (aAddon.userDisabled != gDisableUpdateVersionAddon) {
            aAddon.userDisabled = gDisableUpdateVersionAddon;
          }
        }
      });
      // Start the timout timer before the update window is displayed so it can
      // clean up tests that don't successfully display the update window.
      setupTimer(gTestTimeout);
      aCallback();
    });
  }

  // If the app.update.test.disabledAddons preference exists the pre-existing
  // add-ons have already been disabled so they don't interfere with the tests,
  // the test add-ons have already been installed, and the only thing that needs
  // to be done is setting the appropriate userDisabled value for the noupdate
  // test add-ons.
  if (Services.prefs.prefHasUserValue(PREF_DISABLEDADDONS)) {
    setNoUpdateAddonsDisabledState();
    return;
  }

  // Disable all pre-existing enabled addons so they don't interfere with the
  // tests.
  AddonManager.getAllAddons(function(aAddons) {
    let disabledAddons = [];
    aAddons.forEach(function(aAddon) {
      // If an addon's type equals plugin it is skipped since
      // checking plugins compatibility information isn't supported at this
      // time (also see bug 566787). Also, SCOPE_APPLICATION add-ons are
      // excluded by app update so there is no reason to disable them.
      if (aAddon.type != "plugin" && !aAddon.appDisabled &&
          !aAddon.userDisabled &&
          aAddon.scope != AddonManager.SCOPE_APPLICATION) {
        disabledAddons.push(aAddon);
        aAddon.userDisabled = true;
      }
    });
    // If there are no pre-existing add-ons the preference value will be an
    // empty string.
    Services.prefs.setCharPref(PREF_DISABLEDADDONS, disabledAddons.join(" "));

    // Install the test add-ons.
    let xpiFiles = getTestAddonXPIFiles();
    let xpiCount = xpiFiles.length;
    let installs = [];
    xpiFiles.forEach(function(aFile) {
      AddonManager.getInstallForFile(aFile, function(aInstall) {
        if (!aInstall) {
          throw "No AddonInstall created for " + aFile.path;
        }

        installs.push(aInstall);

        if (--xpiCount == 0) {
          let installCount = installs.length;
          let installCompleted = function(aInstall) {
            aInstall.removeListener(listener);

            if (getAddonTestType(aInstall.addon.name) == "userdisabled") {
              aInstall.addon.userDisabled = true;
            }
            if (--installCount == 0) {
              setNoUpdateAddonsDisabledState();
            }
          };

          let listener = {
            onDownloadFailed: installCompleted,
            onDownloadCancelled: installCompleted,
            onInstallFailed: installCompleted,
            onInstallCancelled: installCompleted,
            onInstallEnded: installCompleted
          };

          installs.forEach(function(aInstall) {
            aInstall.addListener(listener);
            aInstall.install();
          });
        }
      });
    });
  });
}

/**
 * Uninstalls the test add-ons, enables add-ons that were disabled when the
 * test started, and calls the callback specified in the aCallback parameter.
 *
 * @param  aCallback
 *         A callback to call after all operations have completed.
 */
function resetAddons(aCallback) {
  debugDump("entering");
  // If test_9999_cleanup.xul is ran by itself then the test add-ons will not
  // have been installed and any pre-existing add-ons will not have been
  // disabled so return early.
  if (!Services.prefs.prefHasUserValue(PREF_DISABLEDADDONS)) {
    debugDump("preference " + PREF_DISABLEDADDONS + " doesn't exist... " +
              "returning early");
    aCallback();
    return;
  }

  // Uninstall the test add-ons.
  let count = TEST_ADDONS.length;
  function uninstallCompleted(aAddon) {
    if (--count == 0) {
      AddonManager.removeAddonListener(listener);

      // Enable the pre-existing add-ons that were disabled so they wouldn't
      // interfere with the tests.
      let disabledAddons = Services.prefs.getCharPref(PREF_DISABLEDADDONS).split(" ");
      Services.prefs.clearUserPref(PREF_DISABLEDADDONS);
      AddonManager.getAllAddons(function(aAddons) {
        aAddons.forEach(function(aAddon) {
          if (disabledAddons.indexOf(aAddon.id)) {
            aAddon.userDisabled = false;
          }
        });
        aCallback();
      });
    }
  }

  let listener = {
    onUninstalled: uninstallCompleted
  };

  AddonManager.addAddonListener(listener);
  TEST_ADDONS.forEach(function(aName) {
    AddonManager.getAddonByID(aName + ADDON_ID_SUFFIX, function(aAddon) {
      aAddon.uninstall();
    });
  });
}

/**
 * Helper function to get the string before the '_' character in an add-on's
 * name or id which is used to determine the add-on test type used by the tests.
 *
 * @param  aName
 *         The test add-on's name or id.
 * @return The string before the '_' character in the string passed in the aName
 *         parameter.
 */
function getAddonTestType(aName) {
  return aName.split("_")[0];
}

/**
 * Helper function to create add-on xpi files for the default test add-ons.
 *
 * @return An array with each member being an nsILocalFile for an add-on XPI
 *         file.
 */
function getTestAddonXPIFiles() {
  let addonPrepDir = Services.dirsvc.get(NS_APP_USER_PROFILE_50_DIR,
                                         Ci.nsILocalFile);
  addonPrepDir.append(ADDON_PREP_DIR);

  let bootstrap = addonPrepDir.clone();
  bootstrap.append("bootstrap.js");
  // If a previous test has already created bootstrap.js don't create it again.
  if (!bootstrap.exists()) {
    let bootstrapContents = "function install(data, reason){ }\n" +
                            "function startup(data, reason){ }\n" +
                            "function shutdown(data, reason){ }\n" +
                            "function uninstall(data, reason){ }\n";
    writeFile(bootstrap, bootstrapContents);
  }

  let installRDF = addonPrepDir.clone();
  installRDF.append("install.rdf");

  let xpiFiles = [];
  TEST_ADDONS.forEach(function(aName) {
    let xpiFile = addonPrepDir.clone();
    xpiFile.append(aName + ".xpi");

    if (installRDF.exists()) {
      installRDF.remove(false);
    }
    writeFile(installRDF, getInstallRDFString(aName));
    gZipW.open(xpiFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
    gZipW.addEntryFile(installRDF.leafName,
                       Ci.nsIZipWriter.COMPRESSION_DEFAULT, installRDF,
                       false);
    gZipW.addEntryFile(bootstrap.leafName,
                       Ci.nsIZipWriter.COMPRESSION_DEFAULT, bootstrap,
                       false);
    gZipW.close();
    xpiFiles.push(xpiFile);
  });

  return xpiFiles;
}

/**
 * Helper function to gets the string representation of the contents of the
 * add-on's install.rdf file.
 *
 * @param  aName
 *         The string to use for the add-on's name which is also used to
 *         construct the local-part in RFC 5322 format of the add-on's ID.
 * @return A string representation of the contents of the add-on's install.rdf
 *         file.
 */
function getInstallRDFString(aName) {
  let maxVersion = Services.appinfo.platformVersion;
  switch (getAddonTestType(aName)) {
    case "compatible":
      maxVersion = getNewerPlatformVersion();
      break;
    case "appdisabled":
      maxVersion = "0.1";
      break;
  }

  return "<?xml version=\"1.0\"?>\n" +
         "<RDF xmlns=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n" +
         "  xmlns:em=\"http://www.mozilla.org/2004/em-rdf#\">\n" +
         "  <Description about=\"urn:mozilla:install-manifest\">\n" +
         "    <em:id>" + aName + ADDON_ID_SUFFIX + "</em:id>\n" +
         "    <em:version>1.0</em:version>\n" +
         "    <em:bootstrap>true</em:bootstrap>\n" +
         "    <em:name>" + aName + "</em:name>\n" +
         "    <em:description>Test Description</em:description>\n" +
         "    <em:targetApplication>\n" +
         "      <Description>\n" +
         "        <em:id>toolkit@mozilla.org</em:id>\n" +
         "        <em:minVersion>undefined</em:minVersion>\n" +
         "        <em:maxVersion>" + maxVersion + "</em:maxVersion>\n" +
         "      </Description>\n" +
         "    </em:targetApplication>\n" +
         "  </Description>\n" +
         "</RDF>";
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
   *         The value to set the app.update.cert.maxErrors preference to.
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
