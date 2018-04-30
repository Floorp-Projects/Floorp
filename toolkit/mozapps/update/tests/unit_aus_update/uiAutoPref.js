/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const WindowWatcher = {
  openWindow(aParent, aUrl, aName, aFeatures, aArgs) {
    check_showUpdateAvailable();
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher])
};

const WindowMediator = {
  getMostRecentWindow(aWindowType) {
    executeSoon(check_status);
    return { getInterface: ChromeUtils.generateQI([Ci.nsIDOMWindow]) };
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowMediator])
};

function run_test() {
  setupTestCommon();
  // Calling do_get_profile prevents an error from being logged
  do_get_profile();

  debugDump("testing that an update download doesn't start when the " +
            PREF_APP_UPDATE_AUTO + " preference is false");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);

  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  standardInit();

  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           WindowWatcher);
  let windowMediatorCID =
    MockRegistrar.register("@mozilla.org/appshell/window-mediator;1",
                           WindowMediator);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(windowWatcherCID);
    MockRegistrar.unregister(windowMediatorCID);
  });

  let patches = getRemotePatchString({});
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gAUS.notify(null);
}

function check_status() {
  let status = readStatusFile();
  Assert.notEqual(status, STATE_DOWNLOADING,
                  "the update state" + MSG_SHOULD_EQUAL);

  // Pause the download and reload the Update Manager with an empty update so
  // the Application Update Service doesn't write the update xml files during
  // xpcom-shutdown which will leave behind the test directory.
  gAUS.pauseDownload();
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), true);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  reloadUpdateManagerData();

  executeSoon(doTestFinish);
}

function check_showUpdateAvailable() {
  do_throw("showUpdateAvailable should not have called openWindow!");
}
