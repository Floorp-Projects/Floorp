/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://testing-common/AddonManagerTesting.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm", this);

// Lazy load |LightweightThemeManager|, we won't be using it on Gonk.
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");

// Lazy load |ProfileAge| as it is not available on Android.
XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
                                  "resource://gre/modules/ProfileAge.jsm");

// The webserver hosting the addons.
let gHttpServer = null;
// The URL of the webserver root.
let gHttpRoot = null;
// The URL of the data directory, on the webserver.
let gDataRoot = null;

let gNow = new Date(2010, 1, 1, 12, 0, 0);
fakeNow(gNow);

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

// The profile reset date, in milliseconds (Today)
const PROFILE_RESET_DATE_MS = Date.now();
// The profile creation date, in milliseconds (Yesterday).
const PROFILE_CREATION_DATE_MS = PROFILE_RESET_DATE_MS - MILLISECONDS_PER_DAY;

const FLASH_PLUGIN_NAME = "Shockwave Flash";
const FLASH_PLUGIN_DESC = "A mock flash plugin";
const FLASH_PLUGIN_VERSION = "\u201c1.1.1.1\u201d";
const PLUGIN_MIME_TYPE1 = "application/x-shockwave-flash";
const PLUGIN_MIME_TYPE2 = "text/plain";

const PLUGIN2_NAME = "Quicktime";
const PLUGIN2_DESC = "A mock Quicktime plugin";
const PLUGIN2_VERSION = "2.3";

const PERSONA_ID = "3785";
// Defined by LightweightThemeManager, it is appended to the PERSONA_ID.
const PERSONA_ID_SUFFIX = "@personas.mozilla.org";
const PERSONA_NAME = "Test Theme";
const PERSONA_DESCRIPTION = "A nice theme/persona description.";

const PLUGIN_UPDATED_TOPIC     = "plugins-list-updated";

/**
 * Used to mock plugin tags in our fake plugin host.
 */
function PluginTag(aName, aDescription, aVersion, aEnabled) {
  this.name = aName;
  this.description = aDescription;
  this.version = aVersion;
  this.disabled = !aEnabled;
}

PluginTag.prototype = {
  name: null,
  description: null,
  version: null,
  filename: null,
  fullpath: null,
  disabled: false,
  blocklisted: false,
  clicktoplay: true,

  mimeTypes: [ PLUGIN_MIME_TYPE1, PLUGIN_MIME_TYPE2 ],

  getMimeTypes: function(count) {
    count.value = this.mimeTypes.length;
    return this.mimeTypes;
  }
};

// A container for the plugins handled by the fake plugin host.
let gInstalledPlugins = [
  new PluginTag("Java", "A mock Java plugin", "1.0", false /* Disabled */),
  new PluginTag(FLASH_PLUGIN_NAME, FLASH_PLUGIN_DESC, FLASH_PLUGIN_VERSION, true),
];

// A fake plugin host for testing plugin telemetry environment.
let PluginHost = {
  getPluginTags: function(countRef) {
    countRef.value = gInstalledPlugins.length;
    return gInstalledPlugins;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIPluginHost)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

function registerFakePluginHost() {
  MockRegistrar.register("@mozilla.org/plugin/host;1", PluginHost);
}

/**
 * Used to spoof the Persona Id.
 */
function spoofTheme(aId, aName, aDesc) {
  return {
    id: aId,
    name: aName,
    description: aDesc,
    headerURL: "http://lwttest.invalid/a.png",
    footerURL: "http://lwttest.invalid/b.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };
}

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
  if (gIsAndroid) {
    // ProfileAge is not available on Android.
    return true;
  }

  let profileAccessor = new ProfileAge();

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
    Assert.ok(checkString(data.build[f]), f + " must be a valid string.");
    Assert.equal(data.build[f], expectedInfo[f], f + " must have the correct value.");
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
    isInOptoutSample: "boolean",
    locale: "string",
    update: "object",
    userPrefs: "object",
  };

  Assert.ok("settings" in data, "There must be a settings section in Environment.");

  for (let f in EXPECTED_FIELDS_TYPES) {
    Assert.equal(typeof data.settings[f], EXPECTED_FIELDS_TYPES[f],
                 f + " must have the correct type.");
  }

  // Check "addonCompatibilityCheckEnabled" separately, as it is not available
  // on Gonk.
  if (gIsGonk) {
    Assert.ok(!("addonCompatibilityCheckEnabled" in data.settings), "Must not be available on Gonk.");
  } else {
    Assert.equal(data.settings.addonCompatibilityCheckEnabled, AddonManager.checkCompatibility);
  }

  // Check "isDefaultBrowser" separately, as it is not available on Android an can either be
  // null or boolean on other platforms.
  if (gIsAndroid) {
    Assert.ok(!("isDefaultBrowser" in data.settings), "Must not be available on Android.");
  } else {
    Assert.ok(checkNullOrBool(data.settings.isDefaultBrowser));
  }

  // Check "channel" separately, as it can either be null or string.
  let update = data.settings.update;
  Assert.ok(checkNullOrString(update.channel));
  Assert.equal(typeof update.enabled, "boolean");
  Assert.equal(typeof update.autoDownload, "boolean");

  // Check "defaultSearchEngine" separately, as it can either be undefined or string.
  if ("defaultSearchEngine" in data.settings) {
    checkString(data.settings.defaultSearchEngine);
    Assert.equal(typeof data.settings.defaultSearchEngineData, "object");
  }
}

function checkProfileSection(data) {
  if (gIsAndroid) {
    Assert.ok(!("profile" in data),
              "There must be no profile section in Environment on Android.");
    return;
  }

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
    Assert.equal(data.partner[f], EXPECTED_FIELDS[f], f + " must have the correct value.");
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
    Assert.ok(f in data, f + " must be available.");

    if (data[f]) {
      // Since we have a non-null value, check if it has the correct type.
      Assert.equal(typeof data[f], EXPECTED_ADAPTER_FIELDS_TYPES[f],
                   f + " must have the correct type.");
    }
  }
}

function checkSystemSection(data) {
  const EXPECTED_FIELDS = [ "memoryMB", "cpu", "os", "hdd", "gfx" ];
  const EXPECTED_HDD_FIELDS = [ "profile", "binary", "system" ];

  Assert.ok("system" in data, "There must be a system section in Environment.");

  // Make sure we have all the top level sections and fields.
  for (let f of EXPECTED_FIELDS) {
    Assert.ok(f in data.system, f + " must be available.");
  }

  Assert.ok(Number.isFinite(data.system.memoryMB), "MemoryMB must be a number.");

  if (gIsWindows || gIsMac || gIsLinux) {
    let EXTRA_CPU_FIELDS = ["cores", "model", "family", "stepping",
			    "l2cacheKB", "l3cacheKB", "speedMHz"];
    if (gIsMac || gIsLinux) {
      EXTRA_CPU_FIELDS.push("vendor");
    }

    for (let f of EXTRA_CPU_FIELDS) {
      // Note this is testing TelemetryEnvironment.js only, not that the
      // values are valid - null is the fallback.
      Assert.ok(f in data.system.cpu, f + " must be available under cpu.");
    }

    if (gIsWindows) {
      Assert.equal(typeof data.system.isWow64, "boolean",
             "isWow64 must be available on Windows and have the correct type.");
      Assert.ok("virtualMaxMB" in data.system, "virtualMaxMB must be available.");
      Assert.ok(Number.isFinite(data.system.virtualMaxMB),
                "virtualMaxMB must be a number.");
    }

    // We insist these are available
    for (let f of ["cores"]) {
	Assert.ok(!(f in data.system.cpu) ||
		  Number.isFinite(data.system.cpu[f]),
		  f + " must be a number if non null.");
    }

    // These should be numbers if they are not null
    for (let f of ["model", "family", "stepping", "l2cacheKB",
		   "l3cacheKB", "speedMHz"]) {
	Assert.ok(!(f in data.system.cpu) ||
		  data.system.cpu[f] === null ||
		  Number.isFinite(data.system.cpu[f]),
		  f + " must be a number if non null.");
    }
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
    Assert.ok(checkNullOrString(osData.kernelVersion));
  }

  let check = gIsWindows ? checkString : checkNullOrString;
  for (let disk of EXPECTED_HDD_FIELDS) {
    Assert.ok(check(data.system.hdd[disk].model));
    Assert.ok(check(data.system.hdd[disk].revision));
  }

  let gfxData = data.system.gfx;
  Assert.ok("D2DEnabled" in gfxData);
  Assert.ok("DWriteEnabled" in gfxData);
  // DWriteVersion is disabled due to main thread jank and will be enabled
  // again as part of bug 1154500.
  //Assert.ok("DWriteVersion" in gfxData);
  if (gIsWindows) {
    Assert.equal(typeof gfxData.D2DEnabled, "boolean");
    Assert.equal(typeof gfxData.DWriteEnabled, "boolean");
    // As above, will be enabled again as part of bug 1154500.
    //Assert.ok(checkString(gfxData.DWriteVersion));
  }

  Assert.ok("adapters" in gfxData);
  Assert.ok(gfxData.adapters.length > 0, "There must be at least one GFX adapter.");
  for (let adapter of gfxData.adapters) {
    checkGfxAdapter(adapter);
  }
  Assert.equal(typeof gfxData.adapters[0].GPUActive, "boolean");
  Assert.ok(gfxData.adapters[0].GPUActive, "The first GFX adapter must be active.");

  Assert.ok(Array.isArray(gfxData.monitors));
  if (gIsWindows || gIsMac) {
    Assert.ok(gfxData.monitors.length >= 1, "There is at least one monitor.");
    Assert.equal(typeof gfxData.monitors[0].screenWidth, "number");
    Assert.equal(typeof gfxData.monitors[0].screenHeight, "number");
    if (gIsWindows) {
      Assert.equal(typeof gfxData.monitors[0].refreshRate, "number");
      Assert.equal(typeof gfxData.monitors[0].pseudoDisplay, "boolean");
    }
    if (gIsMac) {
      Assert.equal(typeof gfxData.monitors[0].scale, "number");
    }
  }

  Assert.equal(typeof gfxData.features, "object");
  Assert.equal(typeof gfxData.features.compositor, "string");

  try {
    // If we've not got nsIGfxInfoDebug, then this will throw and stop us doing
    // this test.
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);

    if (gIsWindows || gIsMac) {
      Assert.equal(GFX_VENDOR_ID, gfxData.adapters[0].vendorID);
      Assert.equal(GFX_DEVICE_ID, gfxData.adapters[0].deviceID);
    }

    let features = gfxInfo.getFeatures();
    Assert.equal(features.compositor, gfxData.features.compositor);
  }
  catch (e) {}
}

function checkActiveAddon(data){
  const EXPECTED_ADDON_FIELDS_TYPES = {
    blocklisted: "boolean",
    name: "string",
    userDisabled: "boolean",
    appDisabled: "boolean",
    version: "string",
    scope: "number",
    type: "string",
    foreignInstall: "boolean",
    hasBinaryComponents: "boolean",
    installDay: "number",
    updateDay: "number",
    signedState: mozinfo.addon_signing ? "number" : "undefined",
  };

  for (let f in EXPECTED_ADDON_FIELDS_TYPES) {
    Assert.ok(f in data, f + " must be available.");
    Assert.equal(typeof data[f], EXPECTED_ADDON_FIELDS_TYPES[f],
                 f + " must have the correct type.");
  }

  // We check "description" separately, as it can be null.
  Assert.ok(checkNullOrString(data.description));
}

function checkPlugin(data) {
  const EXPECTED_PLUGIN_FIELDS_TYPES = {
    name: "string",
    version: "string",
    description: "string",
    blocklisted: "boolean",
    disabled: "boolean",
    clicktoplay: "boolean",
    updateDay: "number",
  };

  for (let f in EXPECTED_PLUGIN_FIELDS_TYPES) {
    Assert.ok(f in data, f + " must be available.");
    Assert.equal(typeof data[f], EXPECTED_PLUGIN_FIELDS_TYPES[f],
                 f + " must have the correct type.");
  }

  Assert.ok(Array.isArray(data.mimeTypes));
  for (let type of data.mimeTypes) {
    Assert.ok(checkString(type));
  }
}

function checkTheme(data) {
  // "hasBinaryComponents" is not available when testing.
  const EXPECTED_THEME_FIELDS_TYPES = {
    id: "string",
    blocklisted: "boolean",
    name: "string",
    userDisabled: "boolean",
    appDisabled: "boolean",
    version: "string",
    scope: "number",
    foreignInstall: "boolean",
    installDay: "number",
    updateDay: "number",
  };

  for (let f in EXPECTED_THEME_FIELDS_TYPES) {
    Assert.ok(f in data, f + " must be available.");
    Assert.equal(typeof data[f], EXPECTED_THEME_FIELDS_TYPES[f],
                 f + " must have the correct type.");
  }

  // We check "description" separately, as it can be null.
  Assert.ok(checkNullOrString(data.description));
}

function checkActiveGMPlugin(data) {
  Assert.equal(typeof data.version, "string");
  Assert.equal(typeof data.userDisabled, "boolean");
  Assert.equal(typeof data.applyBackgroundUpdates, "boolean");
}

function checkAddonsSection(data) {
  const EXPECTED_FIELDS = [
    "activeAddons", "theme", "activePlugins", "activeGMPlugins", "activeExperiment",
    "persona",
  ];

  Assert.ok("addons" in data, "There must be an addons section in Environment.");
  for (let f of EXPECTED_FIELDS) {
    Assert.ok(f in data.addons, f + " must be available.");
  }

  // Check the active addons, if available.
  let activeAddons = data.addons.activeAddons;
  for (let addon in activeAddons) {
    checkActiveAddon(activeAddons[addon]);
  }

  // Check "theme" structure.
  if (Object.keys(data.addons.theme).length !== 0) {
    checkTheme(data.addons.theme);
  }

  // Check the active plugins.
  Assert.ok(Array.isArray(data.addons.activePlugins));
  for (let plugin of data.addons.activePlugins) {
    checkPlugin(plugin);
  }

  // Check active GMPlugins
  let activeGMPlugins = data.addons.activeGMPlugins;
  if (!gIsAndroid) {
    // We don't check for data validity on Android here since XPCSHELL tests on Android
    // report one valid (plugin.isValid == true) GMPlugin with a "null" version field.
    for (let gmPlugin in activeGMPlugins) {
      checkActiveGMPlugin(activeGMPlugins[gmPlugin]);
    }
  }

  // Check the active Experiment
  let experiment = data.addons.activeExperiment;
  if (Object.keys(experiment).length !== 0) {
    Assert.ok(checkString(experiment.id));
    Assert.ok(checkString(experiment.branch));
  }

  // Check persona
  Assert.ok(checkNullOrString(data.addons.persona));
}

function checkEnvironmentData(data) {
  checkBuildSection(data);
  checkSettingsSection(data);
  checkProfileSection(data);
  checkPartnerSection(data);
  checkSystemSection(data);
  checkAddonsSection(data);
}

function run_test() {
  // Load a custom manifest to provide search engine loading from JAR files.
  do_load_manifest("chrome.manifest");
  do_test_pending();
  spoofGfxAdapter();
  do_get_profile();
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);

  // Spoof the persona ID, but not on Gonk.
  if (!gIsGonk) {
    LightweightThemeManager.currentTheme =
      spoofTheme(PERSONA_ID, PERSONA_NAME, PERSONA_DESCRIPTION);
  }
  // Register a fake plugin host for consistent flash version data.
  registerFakePluginHost();

  // Setup a webserver to serve Addons, Plugins, etc.
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gDataRoot = gHttpRoot + "data/";
  gHttpServer.registerDirectory("/data/", do_get_cwd());
  do_register_cleanup(() => gHttpServer.stop(() => {}));

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

add_task(function* test_checkEnvironment() {
  let environmentData = yield TelemetryEnvironment.onInitialized();
  checkEnvironmentData(environmentData);
});

add_task(function* test_prefWatchPolicies() {
  const PREF_TEST_1 = "toolkit.telemetry.test.pref_new";
  const PREF_TEST_2 = "toolkit.telemetry.test.pref1";
  const PREF_TEST_3 = "toolkit.telemetry.test.pref2";
  const PREF_TEST_4 = "toolkit.telemetry.test.pref_old";
  const PREF_TEST_5 = "toolkit.telemetry.test.requiresRestart";

  const expectedValue = "some-test-value";
  const unexpectedValue = "unexpected-test-value";
  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST_1, {what: TelemetryEnvironment.RECORD_PREF_VALUE}],
    [PREF_TEST_2, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
    [PREF_TEST_3, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
    [PREF_TEST_4, {what: TelemetryEnvironment.RECORD_PREF_VALUE}],
    [PREF_TEST_5, {what: TelemetryEnvironment.RECORD_PREF_VALUE, requiresRestart: true}],
  ]);

  Preferences.set(PREF_TEST_4, expectedValue);
  Preferences.set(PREF_TEST_5, expectedValue);

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);
  let deferred = PromiseUtils.defer();

  // Check that the pref values are missing or present as expected
  Assert.strictEqual(TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_1], undefined);
  Assert.strictEqual(TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_4], expectedValue);
  Assert.strictEqual(TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_5], expectedValue);

  TelemetryEnvironment.registerChangeListener("testWatchPrefs",
    (reason, data) => deferred.resolve(data));
  let oldEnvironmentData = TelemetryEnvironment.currentEnvironment;

  // Trigger a change in the watched preferences.
  Preferences.set(PREF_TEST_1, expectedValue);
  Preferences.set(PREF_TEST_2, false);
  Preferences.set(PREF_TEST_5, unexpectedValue);
  let eventEnvironmentData = yield deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs");

  // Check environment contains the correct data.
  Assert.deepEqual(oldEnvironmentData, eventEnvironmentData);
  let userPrefs = TelemetryEnvironment.currentEnvironment.settings.userPrefs;

  Assert.equal(userPrefs[PREF_TEST_1], expectedValue,
               "Environment contains the correct preference value.");
  Assert.equal(userPrefs[PREF_TEST_2], "<user-set>",
               "Report that the pref was user set but the value is not shown.");
  Assert.ok(!(PREF_TEST_3 in userPrefs),
            "Do not report if preference not user set.");
  Assert.equal(userPrefs[PREF_TEST_5], expectedValue,
	      "The pref value in the environment data should still be the same");
});

add_task(function* test_prefWatch_prefReset() {
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
  ]);

  // Set the preference to a non-default value.
  Preferences.set(PREF_TEST, false);

  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatchPrefs_reset", deferred.resolve);

  Assert.strictEqual(TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST], "<user-set>");

  // Trigger a change in the watched preferences.
  Preferences.reset(PREF_TEST);
  yield deferred.promise;

  Assert.strictEqual(TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST], undefined);

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_reset");
});

add_task(function* test_addonsWatch_InterestingChange() {
  const ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";
  const ADDON_ID = "tel-restartless-xpi@tests.mozilla.org";
  // We only expect a single notification for each install, uninstall, enable, disable.
  const EXPECTED_NOTIFICATIONS = 4;

  let deferred = PromiseUtils.defer();
  let receivedNotifications = 0;

  let registerCheckpointPromise = (aExpected) => {
    gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
    fakeNow(gNow);
    return new Promise(resolve => TelemetryEnvironment.registerChangeListener(
      "testWatchAddons_Changes" + aExpected, (reason, data) => {
        Assert.equal(reason, "addons-changed");
        receivedNotifications++;
        resolve();
      }));
  };

  let assertCheckpoint = (aExpected) => {
    Assert.equal(receivedNotifications, aExpected);
    TelemetryEnvironment.unregisterChangeListener("testWatchAddons_Changes" + aExpected);
  };

  // Test for receiving one notification after each change.
  let checkpointPromise = registerCheckpointPromise(1);
  yield AddonTestUtils.installXPIFromURL(ADDON_INSTALL_URL);
  yield checkpointPromise;
  assertCheckpoint(1);
  Assert.ok(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons);

  checkpointPromise = registerCheckpointPromise(2);
  let addon = yield AddonTestUtils.getAddonById(ADDON_ID);
  addon.userDisabled = true;
  yield checkpointPromise;
  assertCheckpoint(2);
  Assert.ok(!(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons));

  checkpointPromise = registerCheckpointPromise(3);
  addon.userDisabled = false;
  yield checkpointPromise;
  assertCheckpoint(3);
  Assert.ok(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons);

  checkpointPromise = registerCheckpointPromise(4);
  yield AddonTestUtils.uninstallAddonByID(ADDON_ID);
  yield checkpointPromise;
  assertCheckpoint(4);
  Assert.ok(!(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons));

  Assert.equal(receivedNotifications, EXPECTED_NOTIFICATIONS,
               "We must only receive the notifications we expect.");
});

add_task(function* test_pluginsWatch_Add() {
  if (gIsAndroid) {
    Assert.ok(true, "Skipping: there is no Plugin Manager on Android.");
    return;
  }

  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  Assert.equal(TelemetryEnvironment.currentEnvironment.addons.activePlugins.length, 1);

  let newPlugin = new PluginTag(PLUGIN2_NAME, PLUGIN2_DESC, PLUGIN2_VERSION, true);
  gInstalledPlugins.push(newPlugin);

  let deferred = PromiseUtils.defer();
  let receivedNotifications = 0;
  let callback = (reason, data) => {
    receivedNotifications++;
    Assert.equal(reason, "addons-changed");
    deferred.resolve();
  };
  TelemetryEnvironment.registerChangeListener("testWatchPlugins_Add", callback);

  Services.obs.notifyObservers(null, PLUGIN_UPDATED_TOPIC, null);
  yield deferred.promise;

  Assert.equal(TelemetryEnvironment.currentEnvironment.addons.activePlugins.length, 2);

  TelemetryEnvironment.unregisterChangeListener("testWatchPlugins_Add");

  Assert.equal(receivedNotifications, 1, "We must only receive one notification.");
});

add_task(function* test_pluginsWatch_Remove() {
  if (gIsAndroid) {
    Assert.ok(true, "Skipping: there is no Plugin Manager on Android.");
    return;
  }

  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  // Find the test plugin.
  let plugin = gInstalledPlugins.find(plugin => (plugin.name == PLUGIN2_NAME));
  Assert.ok(plugin, "The test plugin must exist.");

  // Remove it from the PluginHost.
  gInstalledPlugins = gInstalledPlugins.filter(p => p != plugin);

  let deferred = PromiseUtils.defer();
  let receivedNotifications = 0;
  let callback = () => {
    receivedNotifications++;
    deferred.resolve();
  };
  TelemetryEnvironment.registerChangeListener("testWatchPlugins_Remove", callback);

  Services.obs.notifyObservers(null, PLUGIN_UPDATED_TOPIC, null);
  yield deferred.promise;

  TelemetryEnvironment.unregisterChangeListener("testWatchPlugins_Remove");

  Assert.equal(receivedNotifications, 1, "We must only receive one notification.");
});

add_task(function* test_addonsWatch_NotInterestingChange() {
  // We are not interested to dictionary addons changes.
  const DICTIONARY_ADDON_INSTALL_URL = gDataRoot + "dictionary.xpi";
  const INTERESTING_ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";

  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  let receivedNotification = false;
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testNotInteresting",
    () => {
      Assert.ok(!receivedNotification, "Should not receive multiple notifications");
      receivedNotification = true;
      deferred.resolve();
    });

  yield AddonTestUtils.installXPIFromURL(DICTIONARY_ADDON_INSTALL_URL);
  yield AddonTestUtils.installXPIFromURL(INTERESTING_ADDON_INSTALL_URL);

  yield deferred.promise;
  Assert.ok(!("telemetry-dictionary@tests.mozilla.org" in
              TelemetryEnvironment.currentEnvironment.addons.activeAddons),
            "Dictionaries should not appear in active addons.");

  TelemetryEnvironment.unregisterChangeListener("testNotInteresting");
});

add_task(function* test_addonsAndPlugins() {
  const ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";
  const ADDON_ID = "tel-restartless-xpi@tests.mozilla.org";
  const ADDON_INSTALL_DATE = truncateToDays(Date.now());
  const EXPECTED_ADDON_DATA = {
    blocklisted: false,
    description: "A restartless addon which gets enabled without a reboot.",
    name: "XPI Telemetry Restartless Test",
    userDisabled: false,
    appDisabled: false,
    version: "1.0",
    scope: 1,
    type: "extension",
    foreignInstall: false,
    hasBinaryComponents: false,
    installDay: ADDON_INSTALL_DATE,
    updateDay: ADDON_INSTALL_DATE,
    signedState: mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_MISSING : AddonManager.SIGNEDSTATE_NOT_REQUIRED,
  };

  const EXPECTED_PLUGIN_DATA = {
    name: FLASH_PLUGIN_NAME,
    version: FLASH_PLUGIN_VERSION,
    description: FLASH_PLUGIN_DESC,
    blocklisted: false,
    disabled: false,
    clicktoplay: true,
  };

  // Install an addon so we have some data.
  yield AddonTestUtils.installXPIFromURL(ADDON_INSTALL_URL);

  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // Check addon data.
  Assert.ok(ADDON_ID in data.addons.activeAddons, "We must have one active addon.");
  let targetAddon = data.addons.activeAddons[ADDON_ID];
  for (let f in EXPECTED_ADDON_DATA) {
    Assert.equal(targetAddon[f], EXPECTED_ADDON_DATA[f], f + " must have the correct value.");
  }

  // Check theme data.
  let theme = data.addons.theme;
  Assert.equal(theme.id, (PERSONA_ID + PERSONA_ID_SUFFIX));
  Assert.equal(theme.name, PERSONA_NAME);
  Assert.equal(theme.description, PERSONA_DESCRIPTION);

  // Check plugin data.
  Assert.equal(data.addons.activePlugins.length, 1, "We must have only one active plugin.");
  let targetPlugin = data.addons.activePlugins[0];
  for (let f in EXPECTED_PLUGIN_DATA) {
    Assert.equal(targetPlugin[f], EXPECTED_PLUGIN_DATA[f], f + " must have the correct value.");
  }

  // Check plugin mime types.
  Assert.ok(targetPlugin.mimeTypes.find(m => m == PLUGIN_MIME_TYPE1));
  Assert.ok(targetPlugin.mimeTypes.find(m => m == PLUGIN_MIME_TYPE2));
  Assert.ok(!targetPlugin.mimeTypes.find(m => m == "Not There."));

  let personaId = (gIsGonk) ? null : PERSONA_ID;
  Assert.equal(data.addons.persona, personaId, "The correct Persona Id must be reported.");
});

add_task(function* test_signedAddon() {
  const ADDON_INSTALL_URL = gDataRoot + "signed.xpi";
  const ADDON_ID = "tel-signed-xpi@tests.mozilla.org";
  const ADDON_INSTALL_DATE = truncateToDays(Date.now());
  const EXPECTED_ADDON_DATA = {
    blocklisted: false,
    description: "A signed addon which gets enabled without a reboot.",
    name: "XPI Telemetry Signed Test",
    userDisabled: false,
    appDisabled: false,
    version: "1.0",
    scope: 1,
    type: "extension",
    foreignInstall: false,
    hasBinaryComponents: false,
    installDay: ADDON_INSTALL_DATE,
    updateDay: ADDON_INSTALL_DATE,
    signedState: mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_SIGNED : AddonManager.SIGNEDSTATE_NOT_REQUIRED,
  };

  // Set the clock in the future so our changes don't get throttled.
  gNow = fakeNow(futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE));
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("test_signedAddon", deferred.resolve);

  // Install the addon.
  yield AddonTestUtils.installXPIFromURL(ADDON_INSTALL_URL);

  yield deferred.promise;
  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("test_signedAddon");

  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // Check addon data.
  Assert.ok(ADDON_ID in data.addons.activeAddons, "Add-on should be in the environment.");
  let targetAddon = data.addons.activeAddons[ADDON_ID];
  for (let f in EXPECTED_ADDON_DATA) {
    Assert.equal(targetAddon[f], EXPECTED_ADDON_DATA[f], f + " must have the correct value.");
  }
});

add_task(function* test_changeThrottling() {
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
  ]);
  Preferences.reset(PREF_TEST);

  gNow = futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);
  let deferred = PromiseUtils.defer();
  let changeCount = 0;
  TelemetryEnvironment.registerChangeListener("testWatchPrefs_throttling", () => {
    ++changeCount;
    deferred.resolve();
  });

  // The first pref change should trigger a notification.
  Preferences.set(PREF_TEST, 1);
  yield deferred.promise;
  Assert.equal(changeCount, 1);

  // We should only get a change notification for second of the following changes.
  deferred = PromiseUtils.defer();
  gNow = futureDate(gNow, MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);
  Preferences.set(PREF_TEST, 2);
  gNow = futureDate(gNow, 5 * MILLISECONDS_PER_MINUTE);
  fakeNow(gNow);
  Preferences.set(PREF_TEST, 3);
  yield deferred.promise;

  Assert.equal(changeCount, 2);

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_throttling");
});

add_task(function* test_defaultSearchEngine() {
  // Check that no default engine is in the environment before the search service is
  // initialized.
  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.ok(!("defaultSearchEngine" in data.settings));
  Assert.ok(!("defaultSearchEngineData" in data.settings));

  // Load the engines definitions from a custom JAR file: that's needed so that
  // the search provider reports an engine identifier.
  let url = "chrome://testsearchplugin/locale/searchplugins/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url, null, null));

  // Initialize the search service.
  yield new Promise(resolve => Services.search.init(resolve));

  // Our default engine from the JAR file has an identifier. Check if it is correctly
  // reported.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, "telemetrySearchIdentifier");
  let expectedSearchEngineData = {
    name: "telemetrySearchIdentifier",
    loadPath: "jar:[other]/searchTest.jar!testsearchplugin/telemetrySearchIdentifier.xml"
  };
  Assert.deepEqual(data.settings.defaultSearchEngineData, expectedSearchEngineData);

  // Remove all the search engines.
  for (let engine of Services.search.getEngines()) {
    Services.search.removeEngine(engine);
  }
  // The search service does not notify "engine-default" when removing a default engine.
  // Manually force the notification.
  // TODO: remove this when bug 1165341 is resolved.
  Services.obs.notifyObservers(null, "browser-search-engine-modified", "engine-default");

  // Then check that no default engine is reported if none is available.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, "NONE");
  Assert.deepEqual(data.settings.defaultSearchEngineData, {name:"NONE"});

  // Add a new search engine (this will have no engine identifier).
  const SEARCH_ENGINE_ID = "telemetry_default";
  const SEARCH_ENGINE_URL = "http://www.example.org/?search={searchTerms}";
  Services.search.addEngineWithDetails(SEARCH_ENGINE_ID, "", null, "", "get", SEARCH_ENGINE_URL);

  // Set the clock in the future so our changes don't get throttled.
  gNow = fakeNow(futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE));
  // Register a new change listener and then wait for the search engine change to be notified.
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatch_SearchDefault", deferred.resolve);
  Services.search.defaultEngine = Services.search.getEngineByName(SEARCH_ENGINE_ID);
  yield deferred.promise;

  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  const EXPECTED_SEARCH_ENGINE = "other-" + SEARCH_ENGINE_ID;
  Assert.equal(data.settings.defaultSearchEngine, EXPECTED_SEARCH_ENGINE);

  const EXPECTED_SEARCH_ENGINE_DATA = {
    name: "telemetry_default",
    loadPath: "[profile]/searchplugins/telemetrydefault.xml"
  };
  Assert.deepEqual(data.settings.defaultSearchEngineData, EXPECTED_SEARCH_ENGINE_DATA);
  TelemetryEnvironment.unregisterChangeListener("testWatch_SearchDefault");

  // Define and reset the test preference.
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
  ]);
  Preferences.reset(PREF_TEST);

  // Set the clock in the future so our changes don't get throttled.
  gNow = fakeNow(futureDate(gNow, 10 * MILLISECONDS_PER_MINUTE));
  // Watch the test preference.
  TelemetryEnvironment._watchPreferences(PREFS_TO_WATCH);
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testSearchEngine_pref", deferred.resolve);
  // Trigger an environment change.
  Preferences.set(PREF_TEST, 1);
  yield deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("testSearchEngine_pref");

  // Check that the search engine information is correctly retained when prefs change.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, EXPECTED_SEARCH_ENGINE);

  // Check that by default we are not sending a cohort identifier...
  Assert.equal(data.settings.searchCohort, undefined);

  // ... but that if a cohort identifier is set, we send it.
  Services.prefs.setCharPref("browser.search.cohort", "testcohort");
  Services.obs.notifyObservers(null, "browser-search-service", "init-complete");
  data = TelemetryEnvironment.currentEnvironment;
  Assert.equal(data.settings.searchCohort, "testcohort");
});

add_task(function*() {
  do_test_finished();
});
