/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
testserver.registerDirectory("/data/", do_get_file("../data"));

function load_blocklist(aFile) {
  return new Promise((resolve, reject) => {
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

// Tests that an appDisabled add-on that becomes softBlocked remains disabled
// when becoming appEnabled
add_task(async function test_softblock() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  await promiseInstallWebExtension({
    manifest: {
      name: "Softblocked add-on",
      version: "1.0",
      applications: {
        gecko: {
          id: "softblock1@tests.mozilla.org",
          strict_min_version: "2",
          strict_max_version: "3",
        },
      },
    },
  });
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

  AddonTestUtils.appInfo.platformVersion = "2";
  await promiseRestartManager("2");

  s1 = await promiseAddonByID("softblock1@tests.mozilla.org");

  Assert.ok(s1.softDisabled);
  Assert.ok(!s1.appDisabled);
  Assert.ok(!s1.isActive);
});
