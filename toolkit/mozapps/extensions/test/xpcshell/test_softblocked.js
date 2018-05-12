/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const URI_EXTENSION_BLOCKLIST_DIALOG = "chrome://mozapps/content/extensions/blocklist.xul";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

// Allow insecure updates
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
testserver.registerDirectory("/data/", do_get_file("data"));

// Don't need the full interface, attempts to call other methods will just
// throw which is just fine
var WindowWatcher = {
  openWindow(parent, url, name, features, openArgs) {
    // Should be called to list the newly blocklisted items
    Assert.equal(url, URI_EXTENSION_BLOCKLIST_DIALOG);

    // Simulate auto-disabling any softblocks
    var list = openArgs.wrappedJSObject.list;
    list.forEach(function(aItem) {
      if (!aItem.blocked)
        aItem.disable = true;
    });

    // run the code after the blocklist is closed
    Services.obs.notifyObservers(null, "addon-blocklist-closed");
  },

  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"])
};

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function load_blocklist(aFile) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observer() {
      Services.obs.removeObserver(observer, "blocklist-updated");

      resolve();
    }, "blocklist-updated");

    Services.prefs.setCharPref("extensions.blocklist.url", `http://localhost:${gPort}/data/${aFile}`);
    var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                    getService(Ci.nsITimerCallback);
    blocklist.notify(null);
  });
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  run_next_test();
}

// Tests that an appDisabled add-on that becomes softBlocked remains disabled
// when becoming appEnabled
add_task(async function() {
  await promiseWriteInstallRDFForExtension({
    id: "softblock1@tests.mozilla.org",
    version: "1.0",
    name: "Softblocked add-on",
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "2",
      maxVersion: "3"
    }]
  }, profileDir);

  await promiseStartupManager();

  let s1 = await promiseAddonByID("softblock1@tests.mozilla.org");

  // Make sure to mark it as previously enabled.
  await s1.enable();

  Assert.ok(!s1.softDisabled);
  Assert.ok(s1.appDisabled);
  Assert.ok(!s1.isActive);

  await load_blocklist("test_softblocked1.xml");

  Assert.ok(s1.softDisabled);
  Assert.ok(s1.appDisabled);
  Assert.ok(!s1.isActive);

  await promiseRestartManager("2");

  s1 = await promiseAddonByID("softblock1@tests.mozilla.org");

  Assert.ok(s1.softDisabled);
  Assert.ok(!s1.appDisabled);
  Assert.ok(!s1.isActive);
});
