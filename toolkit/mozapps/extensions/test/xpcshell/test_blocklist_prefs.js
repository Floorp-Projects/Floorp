/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests resetting of preferences in blocklist entry when an add-on is blocked.
// See bug 802434.

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// register static files with server and interpolate port numbers in them
mapFile("/data/test_blocklist_prefs_1.xml", testserver);

const profileDir = gProfD.clone();
profileDir.append("extensions");

// A window watcher to handle the blocklist UI.
// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, args) {
    // Should be called to list the newly blocklisted items
    Assert.equal(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = args.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed");

  },

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

function load_blocklist(aFile, aCallback) {
  Services.obs.addObserver(function observer() {
    Services.obs.removeObserver(observer, "blocklist-updated");

    do_execute_soon(aCallback);
  }, "blocklist-updated");

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

  // Add 2 extensions
  writeInstallRDFForExtension({
    id: "block1@tests.mozilla.org",
    version: "1.0",
    name: "Blocked add-on-1 with to-be-reset prefs",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  writeInstallRDFForExtension({
    id: "block2@tests.mozilla.org",
    version: "1.0",
    name: "Blocked add-on-2 with to-be-reset prefs",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  // Pre-set the preferences that we expect to get reset.
  Services.prefs.setIntPref("test.blocklist.pref1", 15);
  Services.prefs.setIntPref("test.blocklist.pref2", 15);
  Services.prefs.setBoolPref("test.blocklist.pref3", true);
  Services.prefs.setBoolPref("test.blocklist.pref4", true);

  startupManager();

  // Before blocklist is loaded.
  AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                               "block2@tests.mozilla.org"], function([a1, a2]) {
    Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

    Assert.equal(Services.prefs.getIntPref("test.blocklist.pref1"), 15);
    Assert.equal(Services.prefs.getIntPref("test.blocklist.pref2"), 15);
    Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref3"), true);
    Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref4"), true);
    run_test_1();
  });
}

function run_test_1() {
  load_blocklist("test_blocklist_prefs_1.xml", function() {
    restartManager();

    // Blocklist changes should have applied and the prefs must be reset.
    AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                                 "block2@tests.mozilla.org"], function([a1, a2]) {
      Assert.notEqual(a1, null);
      Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      Assert.notEqual(a2, null);
      Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);

      // All these prefs must be reset to defaults.
      Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref1"), false);
      Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref2"), false);
      Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref3"), false);
      Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref4"), false);
      end_test();
    });
  });
}
