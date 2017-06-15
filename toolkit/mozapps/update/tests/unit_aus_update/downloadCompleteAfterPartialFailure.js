/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/MockRegistrar.jsm");

const WindowWatcher = {
  getNewPrompter: function WW_getNewPrompter(aParent) {
    Assert.ok(!aParent,
              "the aParent parameter should not be defined");
    return {
      alert: function WW_GNP_alert(aTitle, aText) {
        let title = getString("updaterIOErrorTitle");
        Assert.equal(aTitle, title,
                     "the ui string for title" + MSG_SHOULD_EQUAL);
        let text = gUpdateBundle.formatStringFromName("updaterIOErrorMsg",
                                                      [Services.appinfo.name,
                                                       Services.appinfo.name], 2);
        Assert.equal(aText, text,
                     "the ui string for message" + MSG_SHOULD_EQUAL);

        doTestFinish();
      }
    };
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowWatcher])
};

function run_test() {
  setupTestCommon();

  debugDump("testing download a complete on partial failure. Calling " +
            "nsIUpdatePrompt::showUpdateError should call getNewPrompter " +
            "and alert on the object returned by getNewPrompter when the " +
            "update.state == " + STATE_FAILED + " and the update.errorCode " +
             "== " + WRITE_ERROR + " (Bug 595059).");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);

  let windowWatcherCID =
    MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                           WindowWatcher);
  do_register_cleanup(() => {
    MockRegistrar.unregister(windowWatcherCID);
  });

  standardInit();

  let patchProps = {url: URL_HOST + "/" + FILE_COMPLETE_MAR,
                    state: STATE_FAILED};
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_FAILED);

  reloadUpdateManagerData();

  let update = gUpdateManager.activeUpdate;
  update.errorCode = WRITE_ERROR;
  let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                 createInstance(Ci.nsIUpdatePrompt);
  prompter.showUpdateError(update);
}
