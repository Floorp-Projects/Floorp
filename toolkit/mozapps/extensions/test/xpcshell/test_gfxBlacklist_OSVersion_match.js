/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether new OS versions are matched properly.
// Uses test_gfxBlacklist_OS.xml

Components.utils.import("resource://testing-common/httpd.js");

var gTestserver = new HttpServer();
gTestserver.start(-1);
gPort = gTestserver.identity.primaryPort;

function get_platform() {
  var xulRuntime = Components.classes["@mozilla.org/xre/app-info;1"]
                             .getService(Components.interfaces.nsIXULRuntime);
  return xulRuntime.OS;
}

function load_blocklist(file) {
  Services.prefs.setCharPref("extensions.blocklist.url", "http://localhost:" +
                             gPort + "/data/" + file);
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
  gfxInfo.spoofDriverVersion("8.52.322.2201");
  gfxInfo.spoofVendorID("0xabcd");
  gfxInfo.spoofDeviceID("0x1234");

  // Spoof the version of the OS appropriately to test the test file.
  switch (get_platform()) {
    case "WINNT":
      // Windows 8
      gfxInfo.spoofOSVersion(0x60002);
      break;
    case "Linux":
      // We don't have any OS versions on Linux, just "Linux".
      do_test_finished();
      return;
    case "Darwin":
      // Mountain Lion
      gfxInfo.spoofOSVersion(0x1080);
      break;
    case "Android":
      // On Android, the driver version is used as the OS version (because
      // there's so many of them).
      do_test_finished();
      return;
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  startupManager();

  do_test_pending();

  function checkBlacklist()
  {
    if (get_platform() == "WINNT") {
      var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
      do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    } else if (get_platform() == "Darwin") {
      status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
      do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    }

    gTestserver.stop(do_test_finished);
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    do_execute_soon(checkBlacklist);
  }, "blocklist-data-gfxItems", false);

  load_blocklist("test_gfxBlacklist_OS.xml");
}
