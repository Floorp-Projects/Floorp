/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  setupTestCommon();

  logTestInfo("testing nsIUpdatePrompt notifications should not be displayed " +
              "when showUpdateAvailable is called for an unsupported system " +
              "update when the unsupported notification has already been " +
              "shown (bug 843497)");

  setUpdateURLOverride();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  // The HTTP server is only used for the mar file downloads which is slow
  start_httpserver();

  let registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                            "Fake Window Watcher",
                            "@mozilla.org/embedcomp/window-watcher;1",
                            WindowWatcherFactory);
  registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af56}"),
                            "Fake Window Mediator",
                            "@mozilla.org/appshell/window-mediator;1",
                            WindowMediatorFactory);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED, true);
  // This preference is used to determine when the background update check has
  // completed since a successful check will clear the preference.
  Services.prefs.setIntPref(PREF_APP_UPDATE_BACKGROUNDERRORS, 1);

  gResponseBody = getRemoteUpdatesXMLString("  <update type=\"major\" " +
                                            "name=\"Unsupported Update\" " +
                                            "unsupported=\"true\" " +
                                            "detailsURL=\"" + URL_HOST +
                                            "\"></update>\n");
  gAUS.notify(null);
  do_execute_soon(check_test);
}

function check_test() {
  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
    do_execute_soon(check_test);
    return;
  }
  do_check_true(true);

  doTestFinish();
}

function end_test() {
  let registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                              WindowWatcherFactory);
  registrar.unregisterFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af56}"),
                              WindowMediatorFactory);
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = 400;
  gXHR.responseText = gResponseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  } catch (e) {
  }
  var e = { target: gXHR };
  gXHR.onload(e);
}

function check_showUpdateAvailable() {
  do_throw("showUpdateAvailable should not have called openWindow!");
}

var WindowWatcher = {
  openWindow: function(aParent, aUrl, aName, aFeatures, aArgs) {
    check_showUpdateAvailable();
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(AUS_Ci.nsIWindowWatcher) ||
        aIID.equals(AUS_Ci.nsISupports))
      return this;

    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(aOuter, aIID) {
    if (aOuter != null)
      throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(aIID);
  }
};

var WindowMediator = {
  getMostRecentWindow: function(aWindowType) {
    return null;
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(AUS_Ci.nsIWindowMediator) ||
        aIID.equals(AUS_Ci.nsISupports))
      return this;

    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowMediatorFactory = {
  createInstance: function createInstance(aOuter, aIID) {
    if (aOuter != null)
      throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
    return WindowMediator.QueryInterface(aIID);
  }
};
