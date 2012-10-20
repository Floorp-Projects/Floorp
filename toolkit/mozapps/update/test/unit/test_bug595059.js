/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Bug 595059 - begin download of a complete update after a failure applying a
 * partial update.
 */

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);

  logTestInfo("testing Bug 595059 - calling nsIUpdatePrompt::showUpdateError " +
              "should call getNewPrompter and alert on the object returned " +
              "by getNewPrompter when the update.state = " + STATE_FAILED +
              " and the update.errorCode = " + WRITE_ERROR);

  removeUpdateDirsAndFiles();

  let registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                            "Fake Window Watcher",
                            "@mozilla.org/embedcomp/window-watcher;1",
                            WindowWatcherFactory);

  standardInit();

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let url = URL_HOST + URL_PATH + "/complete.mar";
  let patches = getLocalPatchString("complete", url, null, null, null, null,
                                    STATE_FAILED);
  let updates = getLocalUpdateString(patches, null, null, "version 1.0", "1.0",
                                     null, null, null, null, url);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_FAILED);

  reloadUpdateManagerData();

  let update = gUpdateManager.activeUpdate;
  update.errorCode = WRITE_ERROR;
  let prompter = AUS_Cc["@mozilla.org/updates/update-prompt;1"].
                 createInstance(AUS_Ci.nsIUpdatePrompt);
  prompter.showUpdateError(update);
}

function end_test() {
  let registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                              WindowWatcherFactory);
  cleanUp();
}

var WindowWatcher = {
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
        do_test_finished();
      }
    }; 
  },

  QueryInterface: function(iid) {
    if (iid.equals(AUS_Ci.nsIWindowWatcher) ||
        iid.equals(AUS_Ci.nsISupports))
      return this;

    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};
