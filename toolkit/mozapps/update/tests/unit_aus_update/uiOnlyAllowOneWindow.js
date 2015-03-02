/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that nsIUpdatePrompt doesn't display UI for showUpdateInstalled and
 * showUpdateAvailable when there is already an application update window open.
 */

function run_test() {
  setupTestCommon();

  logTestInfo("testing nsIUpdatePrompt notifications should not be seen when " +
              "there is already an application update window open");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);

  let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                            "Fake Window Watcher",
                            "@mozilla.org/embedcomp/window-watcher;1",
                            WindowWatcherFactory);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af56}"),
                            "Fake Window Mediator",
                            "@mozilla.org/appshell/window-mediator;1",
                            WindowMediatorFactory);

  standardInit();

  logTestInfo("testing showUpdateInstalled should not call openWindow");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SHOW_INSTALLED_UI, true);

  gCheckFunc = check_showUpdateInstalled;
  gUP.showUpdateInstalled();
  // Report a successful check after the call to showUpdateInstalled since it
  // didn't throw and otherwise it would report no tests run.
  do_check_true(true);

  logTestInfo("testing showUpdateAvailable should not call openWindow");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(""), false);
  let patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_FAILED);
  let updates = getLocalUpdateString(patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_FAILED);
  reloadUpdateManagerData();

  gCheckFunc = check_showUpdateAvailable;
  let update = gUpdateManager.activeUpdate;
  gUP.showUpdateAvailable(update);
  // Report a successful check after the call to showUpdateAvailable since it
  // didn't throw and otherwise it would report no tests run.
  do_check_true(true);

  registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                              WindowWatcherFactory);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af56}"),
                              WindowMediatorFactory);

  doTestFinish();
}

function check_showUpdateInstalled() {
  do_throw("showUpdateInstalled should not have called openWindow!");
}

function check_showUpdateAvailable() {
  do_throw("showUpdateAvailable should not have called openWindow!");
}

const WindowWatcher = {
  openWindow: function(aParent, aUrl, aName, aFeatures, aArgs) {
    gCheckFunc();
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

const WindowMediator = {
  getMostRecentWindow: function(aWindowType) {
    return { getInterface: XPCOMUtils.generateQI([Ci.nsIDOMWindow]) };
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWindowMediator])
}

const WindowMediatorFactory = {
  createInstance: function createInstance(aOuter, aIID) {
    if (aOuter != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return WindowMediator.QueryInterface(aIID);
  }
};
