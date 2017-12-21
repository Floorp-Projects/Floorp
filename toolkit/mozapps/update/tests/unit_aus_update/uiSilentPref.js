/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const WindowWatcher = {
  openWindow(aParent, aUrl, aName, aFeatures, aArgs) {
    do_throw("should not have called openWindow!");
  },

  getNewPrompter(aParent) {
    do_throw("should not have seen getNewPrompter!");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowWatcher])
};

function run_test() {
  setupTestCommon();

  debugDump("testing nsIUpdatePrompt notifications should not be seen " +
            "when the " + PREF_APP_UPDATE_SILENT + " preference is true");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, true);

  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           WindowWatcher);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(windowWatcherCID);
  });

  standardInit();

  logTestInfo("testing showUpdateAvailable should not call openWindow");
  let patchProps = {state: STATE_FAILED};
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_FAILED);
  reloadUpdateManagerData();

  let update = gUpdateManager.activeUpdate;
  gUP.showUpdateAvailable(update);
  // Report a successful check after the call to showUpdateAvailable since it
  // didn't throw and otherwise it would report no tests run.
  Assert.ok(true,
            "calling showUpdateAvailable should not attempt to open a window");

  logTestInfo("testing showUpdateError should not call getNewPrompter");
  update.errorCode = WRITE_ERROR;
  gUP.showUpdateError(update);
  // Report a successful check after the call to showUpdateError since it
  // didn't throw and otherwise it would report no tests run.
  Assert.ok(true,
            "calling showUpdateError should not attempt to open a window");

  gUpdateManager.cleanupActiveUpdate();
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  doTestFinish();
}
