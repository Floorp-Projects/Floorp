/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests blocking of extensions by ID, name, creator, homepageURL, updateURL
// and RegExps for each. See bug 897735.

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// register static files with server and interpolate port numbers in them
mapFile("/data/test_blocklist_metadata_filters_1.xml", testserver);

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow: function(parent, url, name, features, args) {
    // Should be called to list the newly blocklisted items
    do_check_eq(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = args.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed", null);

  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

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

  // Should get blocked by name
  writeInstallRDFForExtension({
    id: "block1@tests.mozilla.org",
    version: "1.0",
    name: "Mozilla Corp.",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  // Should get blocked by all the attributes.
  writeInstallRDFForExtension({
    id: "block2@tests.mozilla.org",
    version: "1.0",
    name: "Moz-addon",
    creator: "Dangerous",
    homepageURL: "www.extension.dangerous.com",
    updateURL: "www.extension.dangerous.com/update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  // Fails to get blocked because of a different ID even though other
  // attributes match against a blocklist entry.
  writeInstallRDFForExtension({
    id: "block3@tests.mozilla.org",
    version: "1.0",
    name: "Moz-addon",
    creator: "Dangerous",
    homepageURL: "www.extensions.dangerous.com",
    updateURL: "www.extension.dangerous.com/update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  startupManager();

  AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                               "block2@tests.mozilla.org",
                               "block3@tests.mozilla.org"], function([a1, a2, a3]) {
    do_check_eq(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    do_check_eq(a2.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
    do_check_eq(a3.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

    run_test_1();
  });
}

function run_test_1() {
  load_blocklist("test_blocklist_metadata_filters_1.xml", function() {
    restartManager();

    AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                                 "block2@tests.mozilla.org",
                                 "block3@tests.mozilla.org"], function([a1, a2, a3]) {
      do_check_eq(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
      do_check_eq(a2.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);
      do_check_eq(a3.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
      end_test();
    });
  });
}
