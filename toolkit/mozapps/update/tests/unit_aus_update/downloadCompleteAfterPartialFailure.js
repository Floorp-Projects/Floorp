/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon();

  logTestInfo("testing download a complete on partial failure. Calling " +
              "nsIUpdatePrompt::showUpdateError should call getNewPrompter " +
              "and alert on the object returned by getNewPrompter when the " +
              "update.state == " + STATE_FAILED + " and the update.errorCode " +
              "== " + WRITE_ERROR + " (Bug 595059).");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);

  let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                            "Fake Window Watcher",
                            "@mozilla.org/embedcomp/window-watcher;1",
                            WindowWatcherFactory);

  standardInit();

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let url = URL_HOST + "/" + FILE_COMPLETE_MAR;
  let patches = getLocalPatchString("complete", url, null, null, null, null,
                                    STATE_FAILED);
  let updates = getLocalUpdateString(patches, null, null, "version 1.0", "1.0",
                                     null, null, null, null, url);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_FAILED);

  reloadUpdateManagerData();

  let update = gUpdateManager.activeUpdate;
  update.errorCode = WRITE_ERROR;
  let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                 createInstance(Ci.nsIUpdatePrompt);
  prompter.showUpdateError(update);
}

function end_test() {
  let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                              WindowWatcherFactory);
}

const WindowWatcher = {
  getNewPrompter: function(aParent) {
    do_check_eq(aParent, null);
    return {
      alert: function(aTitle, aText) {
        let title = getString("updaterIOErrorTitle");
        do_check_eq(aTitle, title);
        let text = gUpdateBundle.formatStringFromName("updaterIOErrorMsg",
                                                      [Services.appinfo.name,
                                                       Services.appinfo.name], 2);
        do_check_eq(aText, text);

        doTestFinish();
      }
    }; 
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowWatcher])
};

const WindowWatcherFactory = {
  createInstance: function createInstance(aOuter, aIID) {
    if (aOuter != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return WindowWatcher.QueryInterface(aIID);
  }
};
