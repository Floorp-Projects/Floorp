/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether a machine which differs only on device ID, but otherwise
// exactly matches the blacklist entry, is not blocked.
// Uses test_gfxBlacklist.xml

Components.utils.import("resource://testing-common/httpd.js");

var gTestserver = null;

function get_platform() {
  var xulRuntime = Components.classes["@mozilla.org/xre/app-info;1"]
                             .getService(Components.interfaces.nsIXULRuntime);
  return xulRuntime.OS;
}

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:4444/data/" + file);
  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService(Ci.nsITimerCallback);
  blocklist.notify(null);
}

// Performs the initial setup
function run_test() {
  try {
    var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
  } catch (e) {
    do_test_finished();
    return;
  }

  // We can't do anything if we can't spoof the stuff we need.
  if (!(gfxInfo instanceof Ci.nsIGfxInfoDebug)) {
    do_test_finished();
    return;
  }

  gfxInfo.QueryInterface(Ci.nsIGfxInfoDebug);

  // Set the vendor/device ID, etc, to match the test file.
  switch (get_platform()) {
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
      gfxInfo.spoofOSVersion(0x1050);
      break;
    case "Android":
      gfxInfo.spoofVendorID("abcd");
      gfxInfo.spoofDeviceID("aabb");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupManager();

  gTestserver = new HttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("data"));
  gTestserver.start(4444);

  do_test_pending();

  function checkBlacklist()
  {
    var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_NO_INFO);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_NO_INFO);

    gTestserver.stop(do_test_finished);
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    do_execute_soon(checkBlacklist);
  }, "blocklist-data-gfxItems", false);

  load_blocklist("test_gfxBlacklist.xml");
}
