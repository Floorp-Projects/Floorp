/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This should eventually be moved to head_addons.js
var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// Test whether a machine which differs only on device ID, but otherwise
// exactly matches the blacklist entry, is not blocked.
// Uses test_gfxBlacklist.xml

Cu.import("resource://testing-common/httpd.js");

var gTestserver = new HttpServer();
gTestserver.start(-1);
gPort = gTestserver.identity.primaryPort;
mapFile("/data/test_gfxBlacklist.xml", gTestserver);

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + file);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

// Performs the initial setup
function run_test() {
  var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

  // We can't do anything if we can't spoof the stuff we need.
  if (!(gfxInfo instanceof Ci.nsIGfxInfoDebug)) {
    do_test_finished();
    return;
  }

  gfxInfo.QueryInterface(Ci.nsIGfxInfoDebug);

  // Set the vendor/device ID, etc, to match the test file.
  switch (Services.appinfo.OS) {
    case "WINNT":
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x9876");
      gfxInfo.spoofDriverVersion("8.52.322.2201");
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x9876");
      break;
    case "Darwin":
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x9876");
      gfxInfo.spoofOSVersion(0x1090);
      break;
    case "Android":
      gfxInfo.spoofVendorID("abcd");
      gfxInfo.spoofDeviceID("aabb");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupManager();

  do_test_pending();

  function checkBlacklist() {
    var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_CANVAS2D_ACCELERATION);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    gTestserver.stop(do_test_finished);
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    do_execute_soon(checkBlacklist);
  }, "blocklist-data-gfxItems");

  load_blocklist("test_gfxBlacklist.xml");
}
