/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/services/healthreport/profile.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);


const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";
const APP_HOTFIX_VERSION = "2.3.4a";

const DISTRIBUTION_ID = "distributor-id";
const DISTRIBUTION_VERSION = "4.5.6b";
const DISTRIBUTOR_NAME = "Some Distributor";
const DISTRIBUTOR_CHANNEL = "A Channel";
const PARTNER_NAME = "test";
const PARTNER_ID = "NicePartner-ID-3785";

const GFX_VENDOR_ID = "0xabcd";
const GFX_DEVICE_ID = "0x1234";

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
// The profile reset date, in milliseconds (Today)
const PROFILE_RESET_DATE_MS = Date.now();
// The profile creation date, in milliseconds (Yesterday).
const PROFILE_CREATION_DATE_MS = PROFILE_RESET_DATE_MS - MILLISECONDS_PER_DAY;

const gIsWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
const gIsMac = ("@mozilla.org/xpcom/mac-utils;1" in Cc);
const gIsAndroid =  ("@mozilla.org/android/bridge;1" in Cc);
const gIsGonk = ("@mozilla.org/cellbroadcast/gonkservice;1" in Cc);

function spoofGfxAdapter() {
  try {
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);
    gfxInfo.spoofVendorID(GFX_VENDOR_ID);
    gfxInfo.spoofDeviceID(GFX_DEVICE_ID);
  } catch (x) {
    // If we can't test gfxInfo, that's fine, we'll note it later.
  }
}

function spoofProfileReset() {
  let profileAccessor = new ProfileTimesAccessor();

  return profileAccessor.writeTimes({
    created: PROFILE_CREATION_DATE_MS,
    reset: PROFILE_RESET_DATE_MS
  });
}

function spoofPartnerInfo() {
  let prefsToSpoof = {};
  prefsToSpoof["distribution.id"] = DISTRIBUTION_ID;
  prefsToSpoof["distribution.version"] = DISTRIBUTION_VERSION;
  prefsToSpoof["app.distributor"] = DISTRIBUTOR_NAME;
  prefsToSpoof["app.distributor.channel"] = DISTRIBUTOR_CHANNEL;
  prefsToSpoof["app.partner.test"] = PARTNER_NAME;
  prefsToSpoof["mozilla.partner.id"] = PARTNER_ID;

  // Spoof the preferences.
  for (let pref in prefsToSpoof) {
    Preferences.set(pref, prefsToSpoof[pref]);
  }
}

function truncateToDays(aMsec) {
  return Math.floor(aMsec / MILLISECONDS_PER_DAY);
}

/**
 * Check that a value is a string and not empty.
 *
 * @param aValue The variable to check.
 * @return True if |aValue| has type "string" and is not empty, False otherwise.
 */
function checkString(aValue) {
  return (typeof aValue == "string") && (aValue != "");
}

/**
 * If value is non-null, check if it's a valid string.
 *
 * @param aValue The variable to check.
 * @return True if it's null or a valid string, false if it's non-null and an invalid
 *         string.
 */
function checkNullOrString(aValue) {
  if (aValue) {
    return checkString(aValue);
  } else if (aValue === null) {
    return true;
  }

  return false;
}

/**
 * If value is non-null, check if it's a boolean.
 *
 * @param aValue The variable to check.
 * @return True if it's null or a valid boolean, false if it's non-null and an invalid
 *         boolean.
 */
function checkNullOrBool(aValue) {
  if (aValue) {
    return (typeof aValue == "boolean");
  } else if (aValue === null) {
    return true;
  }

  return false;
}

function checkBuildSection(data) {
  const expectedInfo = {
    applicationId: APP_ID,
    applicationName: APP_NAME,
    buildId: "2007010101",
    version: APP_VERSION,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  Assert.ok("build" in data, "There must be a build section in Environment.");

  for (let f in expectedInfo) {
    Assert.ok(checkString(data.build[f]));
    Assert.equal(data.build[f], expectedInfo[f]);
  }

  // Make sure architecture and hotfixVersion are in the environment.
  Assert.ok(checkString(data.build.architecture));
  Assert.ok(checkString(data.build.hotfixVersion));
  Assert.equal(data.build.hotfixVersion, APP_HOTFIX_VERSION);

  if (gIsMac) {
    let macUtils = Cc["@mozilla.org/xpcom/mac-utils;1"].getService(Ci.nsIMacUtils);
    if (macUtils && macUtils.isUniversalBinary) {
      Assert.ok(checkString(data.build.architecturesInBinary));
    }
  }
}

function checkSettingsSection(data) {
  const EXPECTED_FIELDS_TYPES = {
    blocklistEnabled: "boolean",
    e10sEnabled: "boolean",
    telemetryEnabled: "boolean",
    locale: "string",
    update: "object",
    userPrefs: "object",
  };

  Assert.ok("settings" in data, "There must be a settings section in Environment.");

  for (let f in EXPECTED_FIELDS_TYPES) {
    Assert.equal(typeof data.settings[f], EXPECTED_FIELDS_TYPES[f]);
  }

  // Check "isDefaultBrowser" separately, as it can either be null or boolean.
  Assert.ok(checkNullOrBool(data.settings.isDefaultBrowser));

  // Check "channel" separately, as it can either be null or string.
  let update = data.settings.update;
  Assert.ok(checkNullOrString(update.channel));
  Assert.equal(typeof update.enabled, "boolean");
  Assert.equal(typeof update.autoDownload, "boolean");
}

function checkProfileSection(data) {
  Assert.ok("profile" in data, "There must be a profile section in Environment.");
  Assert.equal(data.profile.creationDate, truncateToDays(PROFILE_CREATION_DATE_MS));
  Assert.equal(data.profile.resetDate, truncateToDays(PROFILE_RESET_DATE_MS));
}

function checkPartnerSection(data) {
  const EXPECTED_FIELDS = {
    distributionId: DISTRIBUTION_ID,
    distributionVersion: DISTRIBUTION_VERSION,
    partnerId: PARTNER_ID,
    distributor: DISTRIBUTOR_NAME,
    distributorChannel: DISTRIBUTOR_CHANNEL,
  };

  Assert.ok("partner" in data, "There must be a partner section in Environment.");

  for (let f in EXPECTED_FIELDS) {
    Assert.equal(data.partner[f], EXPECTED_FIELDS[f]);
  }

  // Check that "partnerNames" exists and contains the correct element.
  Assert.ok(Array.isArray(data.partner.partnerNames));
  Assert.ok(data.partner.partnerNames.indexOf(PARTNER_NAME) >= 0);
}

function checkGfxAdapter(data) {
  const EXPECTED_ADAPTER_FIELDS_TYPES = {
    description: "string",
    vendorID: "string",
    deviceID: "string",
    subsysID: "string",
    RAM: "number",
    driver: "string",
    driverVersion: "string",
    driverDate: "string",
    GPUActive: "boolean",
  };

  for (let f in EXPECTED_ADAPTER_FIELDS_TYPES) {
    Assert.ok(f in data);

    if (data[f]) {
      // Since we have a non-null value, check if it has the correct type.
      Assert.equal(typeof data[f], EXPECTED_ADAPTER_FIELDS_TYPES[f]);
    }
  }
}

function checkSystemSection(data) {
  const EXPECTED_FIELDS = [ "memoryMB", "cpu", "os", "hdd", "gfx" ];
  const EXPECTED_HDD_FIELDS = [ "profile", "binary", "system" ];

  Assert.ok("system" in data, "There must be a system section in Environment.");

  // Make sure we have all the top level sections and fields.
  for (let f of EXPECTED_FIELDS) {
    Assert.ok(f in data.system);
  }

  Assert.ok(Number.isFinite(data.system.memoryMB), "MemoryMB must be a number.");
  if (gIsWindows) {
    Assert.equal(typeof data.system.isWow64, "boolean",
              "isWow64 must be available on Windows and have the correct type.");
  }

  let cpuData = data.system.cpu;
  Assert.ok(Number.isFinite(cpuData.count), "CPU count must be a number.");
  Assert.ok(Array.isArray(cpuData.extensions), "CPU extensions must be available.");

  // Device data is only available on Android or Gonk.
  if (gIsAndroid || gIsGonk) {
    let deviceData = data.system.device;
    Assert.ok(checkNullOrString(deviceData.model));
    Assert.ok(checkNullOrString(deviceData.manufacturer));
    Assert.ok(checkNullOrString(deviceData.hardware));
    Assert.ok(checkNullOrBool(deviceData.isTablet));
  }

  let osData = data.system.os;
  Assert.ok(checkNullOrString(osData.name));
  Assert.ok(checkNullOrString(osData.version));
  Assert.ok(checkNullOrString(osData.locale));

  // Service pack is only available on Windows.
  if (gIsWindows) {
    Assert.ok(Number.isFinite(osData["servicePackMajor"]),
              "ServicePackMajor must be a number.");
    Assert.ok(Number.isFinite(osData["servicePackMinor"]),
              "ServicePackMinor must be a number.");
  } else if (gIsAndroid || gIsGonk) {
    Assert.ok(checkString(osData.kernelVersion));
  }

  let check = gIsWindows ? checkString : checkNullOrString;
  for (let disk of EXPECTED_HDD_FIELDS) {
    Assert.ok(check(data.system.hdd[disk].model));
    Assert.ok(check(data.system.hdd[disk].revision));
  }

  let gfxData = data.system.gfx;
  Assert.ok("D2DEnabled" in gfxData);
  Assert.ok("DWriteEnabled" in gfxData);
  Assert.ok("DWriteVersion" in gfxData);
  if (gIsWindows) {
    Assert.equal(typeof gfxData.D2DEnabled, "boolean");
    Assert.equal(typeof gfxData.DWriteEnabled, "boolean");
    Assert.ok(checkString(gfxData.DWriteVersion));
  }

  Assert.ok("adapters" in gfxData);
  Assert.ok(gfxData.adapters.length > 0, "There must be at least one GFX adapter.");
  for (let adapter of gfxData.adapters) {
    checkGfxAdapter(adapter);
  }
  Assert.equal(typeof gfxData.adapters[0].GPUActive, "boolean");
  Assert.ok(gfxData.adapters[0].GPUActive, "The first GFX adapter must be active.");

  try {
    // If we've not got nsIGfxInfoDebug, then this will throw and stop us doing
    // this test.
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);

    if (gIsWindows || gIsMac) {
      Assert.equal(GFX_VENDOR_ID, gfxData.adapters[0].vendorID);
      Assert.equal(GFX_DEVICE_ID, gfxData.adapters[0].deviceID);
    }
  }
  catch (e) {}
}

function checkEnvironmentData(data) {
  checkBuildSection(data);
  checkSettingsSection(data);
  checkProfileSection(data);
  checkPartnerSection(data);
  checkSystemSection(data);
}

function run_test() {
  do_test_pending();
  spoofGfxAdapter();
  do_get_profile();
  createAppInfo(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  spoofPartnerInfo();
  // Spoof the the hotfixVersion
  Preferences.set("extensions.hotfix.lastVersion", APP_HOTFIX_VERSION);

  run_next_test();
}

function isRejected(promise) {
  return new Promise((resolve, reject) => {
    promise.then(() => resolve(false), () => resolve(true));
  });
}

add_task(function* asyncSetup() {
  yield spoofProfileReset();
});

add_task(function* test_initAndShutdown() {
  // Check that init and shutdown work properly.
  TelemetryEnvironment.init();
  yield TelemetryEnvironment.shutdown();
  TelemetryEnvironment.init();
  yield TelemetryEnvironment.shutdown();

  // A double init should be silently handled.
  TelemetryEnvironment.init();
  TelemetryEnvironment.init();

  // getEnvironmentData should return a sane result.
  let data = yield TelemetryEnvironment.getEnvironmentData();
  Assert.ok(!!data);

  // The change listener registration should silently fail after shutdown.
  yield TelemetryEnvironment.shutdown();
  TelemetryEnvironment.registerChangeListener("foo", () => {});
  TelemetryEnvironment.unregisterChangeListener("foo");

  // Other calls after shutdown should reject.
  Assert.ok(yield isRejected(TelemetryEnvironment.shutdown()));
  Assert.ok(yield isRejected(TelemetryEnvironment.getEnvironmentData()));
});

add_task(function* test_changeNotify() {
  TelemetryEnvironment.init();

  // Register some listeners
  let results = new Array(4).fill(false);
  for (let i=0; i<results.length; ++i) {
    let k = i;
    TelemetryEnvironment.registerChangeListener("test"+k, () => results[k] = true);
  }
  // Trigger environment change notifications.
  // TODO: test with proper environment changes, not directly.
  TelemetryEnvironment._onEnvironmentChange("foo");
  Assert.ok(results.every(val => val), "All change listeners should have been notified.");
  results.fill(false);
  TelemetryEnvironment._onEnvironmentChange("bar");
  Assert.ok(results.every(val => val), "All change listeners should have been notified.");

  // Unregister listeners
  for (let i=0; i<4; ++i) {
    TelemetryEnvironment.unregisterChangeListener("test"+i);
  }
});

add_task(function* test_checkEnvironment() {
  yield TelemetryEnvironment.init();
  let environmentData = yield TelemetryEnvironment.getEnvironmentData();

  checkEnvironmentData(environmentData);

  yield TelemetryEnvironment.shutdown();
});

add_task(function* test_prefWatchPolicies() {
  const PREF_TEST_1 = "toolkit.telemetry.test.pref_new";
  const PREF_TEST_2 = "toolkit.telemetry.test.pref1";
  const PREF_TEST_3 = "toolkit.telemetry.test.pref2";

  const expectedValue = "some-test-value";

  let prefsToWatch = {};
  prefsToWatch[PREF_TEST_1] = TelemetryEnvironment.RECORD_PREF_VALUE;
  prefsToWatch[PREF_TEST_2] = TelemetryEnvironment.RECORD_PREF_STATE;
  prefsToWatch[PREF_TEST_3] = TelemetryEnvironment.RECORD_PREF_STATE;

  yield TelemetryEnvironment.init();

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(prefsToWatch);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatchPrefs", deferred.resolve);

  // Trigger a change in the watched preferences.
  Preferences.set(PREF_TEST_1, expectedValue);
  Preferences.set(PREF_TEST_2, false);
  yield deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs");

  // Check environment contains the correct data.
  let environmentData = yield TelemetryEnvironment.getEnvironmentData();

  let userPrefs = environmentData.settings.userPrefs;

  Assert.equal(userPrefs[PREF_TEST_1], expectedValue,
               "Environment contains the correct preference value.");
  Assert.equal(userPrefs[PREF_TEST_2], null,
               "Report that the pref was user set and has no value.");
  Assert.ok(!(PREF_TEST_3 in userPrefs),
            "Do not report if preference not user set.");

  yield TelemetryEnvironment.shutdown();
});

add_task(function* test_prefWatch_prefReset() {
  const PREF_TEST = "toolkit.telemetry.test.pref1";

  let prefsToWatch = {};
  prefsToWatch[PREF_TEST] = TelemetryEnvironment.RECORD_PREF_STATE;
  // Set the preference to a non-default value.
  Preferences.set(PREF_TEST, false);

  yield TelemetryEnvironment.init();

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(prefsToWatch);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatchPrefs_reset", deferred.resolve);

  // Trigger a change in the watched preferences.
  Preferences.reset(PREF_TEST);
  yield deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_reset");
  yield TelemetryEnvironment.shutdown();
});

add_task(function*() {
  do_test_finished();
});
