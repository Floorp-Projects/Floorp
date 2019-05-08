/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether the blacklist successfully adds and removes the prefs that store
// its decisions when the remote blacklist is changed.
// Uses test_gfxBlacklist.json and test_gfxBlacklist2.json

// Performs the initial setup
async function run_test() {
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
      gfxInfo.spoofOSVersion(0x1090);
      break;
    case "Android":
      gfxInfo.spoofVendorID("abcd");
      gfxInfo.spoofDeviceID("asdf");
      gfxInfo.spoofDriverVersion("5");
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  function blacklistAdded(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(ensureBlacklistSet);
  }
  function ensureBlacklistSet() {
    var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    // Make sure unrelated features aren't affected
    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    Assert.equal(Services.prefs.getIntPref("gfx.blacklist.direct2d"),
                 Ci.nsIGfxInfo.FEATURE_BLOCKED_DRIVER_VERSION);

    Services.obs.removeObserver(blacklistAdded, "blocklist-data-gfxItems");
    Services.obs.addObserver(blacklistRemoved, "blocklist-data-gfxItems");
    mockGfxBlocklistItems([
      {
        os: "WINNT 6.1",
        vendor: "0xabcd",
        devices: [
          "0x2783",
          "0x2782",
        ],
        feature: " DIRECT2D ",
        featureStatus: " BLOCKED_DRIVER_VERSION ",
        driverVersion: " 8.52.322.2202 ",
        driverVersionComparator: " LESS_THAN ",
      },
      {
        os: "WINNT 6.0",
        vendor: "0xdcba",
        devices: [
          "0x2783",
          "0x1234",
          "0x2782",
        ],
        feature: " DIRECT3D_9_LAYERS ",
        featureStatus: " BLOCKED_DRIVER_VERSION ",
        driverVersion: " 8.52.322.2202 ",
        driverVersionComparator: " LESS_THAN ",
      },
    ]);
  }

  function blacklistRemoved(aSubject, aTopic, aData) {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(ensureBlacklistUnset);
  }
  function ensureBlacklistUnset() {
    var status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT2D);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    // Make sure unrelated features aren't affected
    status = gfxInfo.getFeatureStatus(Ci.nsIGfxInfo.FEATURE_DIRECT3D_9_LAYERS);
    Assert.equal(status, Ci.nsIGfxInfo.FEATURE_STATUS_OK);

    var exists = false;
    try {
      Services.prefs.getIntPref("gfx.blacklist.direct2d");
      exists = true;
    } catch (e) {}

    Assert.ok(!exists);

    do_test_finished();
  }

  Services.obs.addObserver(blacklistAdded, "blocklist-data-gfxItems");
  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlacklist.json");
}
