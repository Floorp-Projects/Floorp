/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether a machine which exactly matches the blocklist entry is
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

  // Save OS in variable since createAppInfo below will change it to "xpcshell".
  const OS = Services.appinfo.OS;
  // Set the vendor/device ID, etc, to match the test file.
  switch (OS) {
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
      gfxInfo.spoofVendorID("0xabcd");
      gfxInfo.spoofDeviceID("0x1234");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "15.0", "8");
  await promiseStartupManager();

  function checkBlocklist() {
    var failureId = {};
    var status;

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT2D,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLOCKLIST_g1");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLOCKLIST_g2");

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

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_OPENGL_LAYERS);
    if (OS == "Android") {
      // OpenGL layers are never blocklisted on Android, despite an entry in test_gfxBlacklist.json.
      // https://searchfox.org/mozilla-central/rev/c1ec9ecbbc7eac698923ffd18c8594aa3e2e9da0/widget/android/GfxInfo.cpp#431-437
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    } else {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    }

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBGL_OPENGL,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLOCKLIST_g11");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBGL_ANGLE,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLOCKLIST_NO_ID");

    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_WEBGL2, failureId);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);
    Assert.equal(failureId.value, "FEATURE_FAILURE_DL_BLOCKLIST_NO_ID");

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_STAGEFRIGHT,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_H264,
      failureId
    );
    if (OS == "Android" && status != Ci.nsIGfxInfo.FEATURE_STATUS_OK) {
      // Hardware acceleration for H.264 varies by device.
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DEVICE);
      Assert.equal(failureId.value, "FEATURE_FAILURE_WEBRTC_H264");
    } else {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    }

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_ENCODE,
      failureId
    );
    if (OS == "Android" && status != Ci.nsIGfxInfo.FEATURE_STATUS_OK) {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DEVICE);
      Assert.equal(failureId.value, "FEATURE_FAILURE_WEBRTC_ENCODE");
    } else {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    }

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_WEBRTC_HW_ACCELERATION_DECODE,
      failureId
    );
    if (OS == "Android" && status != Ci.nsIGfxInfo.FEATURE_STATUS_OK) {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DEVICE);
      Assert.equal(failureId.value, "FEATURE_FAILURE_WEBRTC_DECODE");
    } else {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    }

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_DIRECT3D_11_LAYERS,
      failureId
    );
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    status = gfxInfo.getFeatureStatus(
      Ci.nsIGfxInfo.FEATURE_HARDWARE_VIDEO_DECODING,
      failureId
    );
    if (OS == "Linux" && status != Ci.nsIGfxInfo.FEATURE_STATUS_OK) {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_PLATFORM_TEST);
      Assert.equal(
        failureId.value,
        "FEATURE_FAILURE_VIDEO_DECODING_TEST_FAILED"
      );
    } else {
      Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);
    }

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
    executeSoon(checkBlocklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlacklist_AllOS.json");
}
