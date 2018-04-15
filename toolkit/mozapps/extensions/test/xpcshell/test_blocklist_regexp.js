/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that blocklist entries using RegExp work as expected. This only covers
// behavior specific to RegExp entries - general behavior is already tested
// in test_blocklistchange.js.

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
gPort = testserver.identity.primaryPort;

testserver.registerDirectory("/data/", do_get_file("data"));

const profileDir = gProfD.clone();
profileDir.append("extensions");

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

    executeSoon(aCallback);
  }, "blocklist-updated");

  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + aFile);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  ok(Services.prefs.getBoolPref("services.blocklist.update_enabled"),
                                "Kinto update should be enabled");
  blocklist.notify(null);
}


function end_test() {
  do_test_finished();
}


async function run_test() {
  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  writeInstallRDFForExtension({
    id: "block1@tests.mozilla.org",
    version: "1.0",
    name: "RegExp blocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "3"
    }]
  }, profileDir);

  startupManager();

  let [a1] = await AddonManager.getAddonsByIDs(["block1@tests.mozilla.org"]);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  run_test_1();
}

function run_test_1() {
  load_blocklist("test_blocklist_regexp_1.xml", async function() {
    restartManager();

    let [a1] = await AddonManager.getAddonsByIDs(["block1@tests.mozilla.org"]);
    Assert.notEqual(a1, null);
    Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);

    end_test();
  });
}
