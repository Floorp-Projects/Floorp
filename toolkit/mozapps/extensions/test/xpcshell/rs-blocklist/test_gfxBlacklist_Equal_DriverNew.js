/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether a machine which is newer than the equal
// blacklist entry is allowed.
// Uses test_gfxBlacklist.json

// Performs the initial setup
async function run_test() {
  var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

  // We can't do anything if we can't spoof the stuff we need.
  if (!(gfxInfo instanceof Ci.nsIGfxInfoDebug)) {
    do_test_finished();
    return;
  }

  gfxInfo.QueryInterface(Ci.nsIGfxInfoDebug);
  gfxInfo.fireTestProcess();

  // Set the vendor/device ID, etc, to match the test file.
  switch (Services.appinfo.OS) {
    case "WINNT":
      gfxInfo.spoofVendorID("0xdcdc");
      gfxInfo.spoofDeviceID("0x1234");
      gfxInfo.spoofDriverVersion("8.52.322.1112");
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      // We don't support driver versions on Linux.
      do_test_finished();
      return;
    case "Darwin":
      // We don't support driver versions on OS X.
      do_test_finished();
      return;
    case "Android":
      gfxInfo.spoofVendorID("dcdc");
      gfxInfo.spoofDeviceID("uiop");
      gfxInfo.spoofDriverVersion("6");
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "15.0", "8");
  await promiseStartupManager();

  function checkBlacklist() {
    var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    // Make sure unrelated features aren't affected
    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_ANGLE);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_HARDWARE_VIDEO_DECODING);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_DECODE);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_ENCODE);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_MSAA);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_CANVAS2D_ACCELERATION);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    do_test_finished();
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlacklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlacklist.json");
}
