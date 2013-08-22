/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that blocklist entries using RegExp work as expected. This only covers
// behavior specific to RegExp entries - general behavior is already tested
// in test_blocklistchange.js.

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

Cu.import("resource://testing-common/httpd.js");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// register static files with server and interpolate port numbers in them
mapFile("/data/test_blocklist_regexp_1.xml", testserver);

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = arguments.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    //run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed", null);

  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1",
                          WindowWatcherFactory);


function load_blocklist(aFile, aCallback) {
  Services.obs.addObserver(function() {
    Services.obs.removeObserver(arguments.callee, "blocklist-updated");

    do_execute_soon(aCallback);
  }, "blocklist-updated", false);

  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + aFile);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}


function end_test() {
  testserver.stop(do_test_finished);
}


function run_test() {
  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  writeInstallRDFForExtension({
    id: "block1@tests.mozilla.org",
    version: "1.0",
    name: "RegExp blocked add-on",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  startupManager();

  AddonManager.getAddonsByIDs(["block1@tests.mozilla.org"], function([a1]) {
    do_check_eq(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

    run_test_1();
  });
}

function run_test_1() {
  load_blocklist("test_blocklist_regexp_1.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(["block1@tests.mozilla.org"], function([a1]) {
      // Blocklist contains two entries that will match this addon - ensure
      // that the first one is applied.
      do_check_neq(a1, null);
      do_check_eq(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

      end_test();
    });
  });
}
