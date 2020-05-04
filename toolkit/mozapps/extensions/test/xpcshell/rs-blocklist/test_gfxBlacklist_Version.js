/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether a machine which exactly matches the blacklist entry is
// successfully blocked.
// Uses test_gfxBlacklist_AllOS.json

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
      gfxInfo.spoofOSVersion(0xa0900);
      break;
    case "Android":
      gfxInfo.spoofVendorID("abcd");
      gfxInfo.spoofDeviceID("asdf");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "15.0", "8");
  await promiseStartupManager();

  function checkBlacklist() {
    var failureId = {};
    var status;

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT2D,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLACKLIST_g1");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLACKLIST_g2");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_10_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    Assert.equal(failureId.value, "");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_10_1_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    Assert.equal(failureId.value, "");

    // These four pass on Linux independent of the blocklist XML file as the
    // try machines don't have support.
    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLACKLIST_g11");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLACKLIST_NO_ID");

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL2, failureId);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLACKLIST_NO_ID");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBGL_MSAA,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_STAGEFRIGHT,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_H264,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_ENCODE,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_DECODE,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_HARDWARE_VIDEO_DECODING,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_ANGLE,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DX_INTEROP2,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    do_test_finished();
  }

  Services.obs.addObserver(function(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlacklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlacklist_AllOS.json");
}
