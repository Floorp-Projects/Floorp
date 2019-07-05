/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests resetting of preferences in blocklist entry when an add-on is blocked.
// See bug 802434.

const URI_EXTENSION_BLOCKLIST_DIALOG =
  "chrome://mozapps/content/extensions/blocklist.xul";

var testserver = createHttpServer({ hosts: ["example.com"] });
gPort = testserver.identity.primaryPort;

testserver.registerDirectory("/data/", do_get_file("../data"));

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
      if (!aItem.blocked) {
        aItem.disable = true;
      }
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed");
  },

  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),
};

MockRegistrar.register(
  "@mozilla.org/embedcomp/window-watcher;1",
  WindowWatcher
);

function load_blocklist(aFile) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, "addon-blocklist-updated");
      resolve();
    }, "addon-blocklist-updated");

    Services.prefs.setCharPref(
      "extensions.blocklist.url",
      `http://localhost:${gPort}/data/${aFile}`
    );
    var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].getService(
      Ci.nsITimerCallback
    );
    blocklist.notify(null);
  });
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  await promiseStartupManager();

  // Add 2 extensions
  await promiseInstallWebExtension({
    manifest: {
      name: "Blocked add-on-1 with to-be-reset prefs",
      version: "1.0",
      applications: { gecko: { id: "block1@tests.mozilla.org" } },
    },
  });
  await promiseInstallWebExtension({
    manifest: {
      name: "Blocked add-on-2 with to-be-reset prefs",
      version: "1.0",
      applications: { gecko: { id: "block2@tests.mozilla.org" } },
    },
  });

  // Pre-set the preferences that we expect to get reset.
  Services.prefs.setIntPref("test.blocklist.pref1", 15);
  Services.prefs.setIntPref("test.blocklist.pref2", 15);
  Services.prefs.setBoolPref("test.blocklist.pref3", true);
  Services.prefs.setBoolPref("test.blocklist.pref4", true);

  // Before blocklist is loaded.
  let [a1, a2] = await AddonManager.getAddonsByIDs([
    "block1@tests.mozilla.org",
    "block2@tests.mozilla.org",
  ]);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  Assert.equal(Services.prefs.getIntPref("test.blocklist.pref1"), 15);
  Assert.equal(Services.prefs.getIntPref("test.blocklist.pref2"), 15);
  Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref3"), true);
  Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref4"), true);
});

add_task(async function test_blocks() {
  await load_blocklist("test_blocklist_prefs_1.xml");

  // Blocklist changes should have applied and the prefs must be reset.
  let [a1, a2] = await AddonManager.getAddonsByIDs([
    "block1@tests.mozilla.org",
    "block2@tests.mozilla.org",
  ]);
  Assert.notEqual(a1, null);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  Assert.notEqual(a2, null);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);

  // All these prefs must be reset to defaults.
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref1"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref2"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref3"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref4"), false);
});
