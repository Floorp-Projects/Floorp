/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This should eventually be moved to head_addons.js
// Test whether old OS versions are not matched when the blacklist contains
// only new OS versions.
// Uses test_gfxBlacklist_OS.xml

var gTestserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
gPort = gTestserver.identity.primaryPort;
gTestserver.registerDirectory("/data/", do_get_file("data"));

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + file);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

// Performs the initial setup
async function run_test() {
  var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

  // We can't do anything if we can't spoof the stuff we need.
  if (!(gfxInfo instanceof Ci.nsIGfxInfoDebug)) {
    do_test_finished();
    return;
  }

  gfxInfo.QueryInterface(Ci.nsIGfxInfoDebug);

  // Set the vendor/device ID, etc, to match the test file.
  gfxInfo.spoofDriverVersion("8.52.322.2201");
  gfxInfo.spoofVendorID("0xabcd");
  gfxInfo.spoofDeviceID("0x1234");

  // Spoof the version of the OS appropriately to test the test file.
  switch (Services.appinfo.OS) {
    case "WINNT":
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      // We don't have any OS versions on Linux, just "Linux".
      do_test_finished();
      return;
    case "Darwin":
      // Lion
      gfxInfo.spoofOSVersion(0x1080);
      break;
    case "Android":
      // On Android, the driver version is used as the OS version (because
      // there's so many of them).
      do_test_finished();
      return;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  function checkBlacklist() {
    if (Services.appinfo.OS == "WINNT") {
      var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    } else if (Services.appinfo.OS == "Darwin") {
      status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    }

    do_test_finished();
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlacklist);
  }, "blocklist-data-gfxItems");

  load_blocklist("test_gfxBlacklist_OSVersion.xml");
}
