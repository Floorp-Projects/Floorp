/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// Test whether a machine which exactly matches the blacklist entry is
// successfully blocked.
// Uses test_gfxBlacklist_AllOS.xml

Cu.import("resource://testing-common/httpd.js");

var gTestserver = new HttpServer();
gTestserver.start(-1);
gPort = gTestserver.identity.primaryPort;
mapFile("/data/test_gfxBlacklist_AllOS.xml", gTestserver);

function get_platform() {
  var xulRuntime = Cc["@mozilla.org/xre/app-info;1"]
                             .getService(Ci.nsIXULRuntime);
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
  var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

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
      gfxInfo.spoofDeviceID("0x1234");
      gfxInfo.spoofDriverVersion("8.52.322.2201");
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x1234");
      break;
    case "Darwin":
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x1234");
      gfxInfo.spoofOSVersion(0x1060);
      break;
    case "Android":
      gfxInfo.spoofVendorID("abcd");
      gfxInfo.spoofDeviceID("asdf");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "15.0", "8");
  startupManager();

  do_test_pending();

  function checkBlacklist()
  {
    var status;

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_10_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_10_1_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    // These four pass on Linux independent of the blocklist XML file as the
    // try machines don't have support.
    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_MSAA);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_STAGEFRIGHT);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_ENCODE);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_DECODE);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_LAYERS);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_HARDWARE_VIDEO_DECODING);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_ANGLE);
    do_check_eq(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    gTestserver.stop(do_test_finished);
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    do_execute_soon(checkBlacklist);
  }, "blocklist-data-gfxItems", false);

  load_blocklist("test_gfxBlacklist_AllOS.xml");
}
