/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests is modifying a file in an unpacked extension
// causes cache invalidation.

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
// Allow the mismatch UI to show
Services.prefs.setBoolPref("extensions.showMismatchUI", true);

const Ci = Components.interfaces;
const extDir = gProfD.clone();
extDir.append("extensions");

var gCachePurged = false;

// Override the window watcher
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    do_check_false(gCachePurged);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(AM_Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

/**
 * Start the test by installing extensions.
 */
function run_test() {
  do_test_pending();
  gCachePurged = false;

  let obs = AM_Cc["@mozilla.org/observer-service;1"].
    getService(AM_Ci.nsIObserverService);
  obs.addObserver({
    observe: function(aSubject, aTopic, aData) {
      gCachePurged = true;
    }
  }, "startupcache-invalidate", false);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();
  // nsAppRunner takes care of clearing this when a new app is installed
  do_check_false(gCachePurged);

  installAllFiles([do_get_addon("test_bug594058")], function() {
    restartManager();
    do_check_true(gCachePurged);
    gCachePurged = false;

    // Now, make it look like we've updated the file. First, start the EM
    // so it records the bogus old time, then update the file and restart.
    let extFile = extDir.clone();
    let pastTime = extFile.lastModifiedTime - 5000;
    extFile.append("bug594058@tests.mozilla.org");
    setExtensionModifiedTime(extFile, pastTime);
    let otherFile = extFile.clone();
    otherFile.append("directory");
    otherFile.lastModifiedTime = pastTime;
    otherFile.append("file1");
    otherFile.lastModifiedTime = pastTime;

    restartManager();
    gCachePurged = false;

    otherFile.lastModifiedTime = pastTime + 5000;
    restartManager();
    do_check_true(gCachePurged);
    gCachePurged = false;

    restartManager();
    do_check_false(gCachePurged);

    do_test_finished();
  });  
}
