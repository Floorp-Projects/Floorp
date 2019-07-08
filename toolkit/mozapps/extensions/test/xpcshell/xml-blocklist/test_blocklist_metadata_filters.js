/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests blocking of extensions by ID, name, creator, homepageURL, updateURL
// and RegExps for each. See bug 897735.

const URI_EXTENSION_BLOCKLIST_DIALOG =
  "chrome://mozapps/content/extensions/blocklist.xul";

var testserver = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
gPort = testserver.identity.primaryPort;

testserver.registerDirectory("/data/", do_get_file("../data"));

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

  // Should get blocked by name
  await promiseInstallWebExtension({
    manifest: {
      name: "Mozilla Corp.",
      version: "1.0",
      applications: { gecko: { id: "block1@tests.mozilla.org" } },
    },
  });

  // Should get blocked by all the attributes.
  await promiseInstallWebExtension({
    manifest: {
      name: "Moz-addon",
      version: "1.0",
      homepage_url: "https://www.extension.dangerous.com/",
      applications: {
        gecko: {
          id: "block2@tests.mozilla.org",
          update_url: "https://www.extension.dangerous.com/update.json",
        },
      },
    },
  });

  // Fails to get blocked because of a different ID even though other
  // attributes match against a blocklist entry.
  await promiseInstallWebExtension({
    manifest: {
      name: "Moz-addon",
      version: "1.0",
      homepage_url: "https://www.extension.dangerous.com/",
      applications: {
        gecko: {
          id: "block3@tests.mozilla.org",
          update_url: "https://www.extension.dangerous.com/update.json",
        },
      },
    },
  });

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([
    "block1@tests.mozilla.org",
    "block2@tests.mozilla.org",
    "block3@tests.mozilla.org",
  ]);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(a3.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});

add_task(async function test_blocks() {
  await load_blocklist("test_blocklist_metadata_filters_1.xml");

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs([
    "block1@tests.mozilla.org",
    "block2@tests.mozilla.org",
    "block3@tests.mozilla.org",
  ]);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);
  Assert.equal(a3.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
});
