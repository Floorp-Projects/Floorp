/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Assert: "resource://testing-common/Assert.jsm",
  // AttributionCode is only needed for Firefox
  AttributionCode: "resource:///modules/AttributionCode.jsm",
  CommonUtils: "resource://services-common/utils.js",
  MockRegistrar: "resource://testing-common/MockRegistrar.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
});

var EXPORTED_SYMBOLS = ["TelemetryEnvironmentTesting"];

const gIsWindows = AppConstants.platform == "win";
const gIsMac = AppConstants.platform == "macosx";
const gIsAndroid = AppConstants.platform == "android";
const gIsLinux = AppConstants.platform == "linux";

const MILLISECONDS_PER_MINUTE = 60 * 1000;
const MILLISECONDS_PER_HOUR = 60 * MILLISECONDS_PER_MINUTE;
const MILLISECONDS_PER_DAY = 24 * MILLISECONDS_PER_HOUR;

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

const DISTRIBUTION_ID = "distributor-id";
const DISTRIBUTION_VERSION = "4.5.6b";
const DISTRIBUTOR_NAME = "Some Distributor";
const DISTRIBUTOR_CHANNEL = "A Channel";
const PARTNER_NAME = "test";
const PARTNER_ID = "NicePartner-ID-3785";

// The profile reset date, in milliseconds (Today)
const PROFILE_RESET_DATE_MS = Date.now();
// The profile creation date, in milliseconds (Yesterday).
const PROFILE_FIRST_USE_MS = PROFILE_RESET_DATE_MS - MILLISECONDS_PER_DAY;
const PROFILE_CREATION_DATE_MS = PROFILE_FIRST_USE_MS - MILLISECONDS_PER_DAY;

const GFX_VENDOR_ID = "0xabcd";
const GFX_DEVICE_ID = "0x1234";

const EXPECTED_HDD_FIELDS = ["profile", "binary", "system"];

// Valid attribution code to write so that settings.attribution can be tested.
const ATTRIBUTION_CODE = "source%3Dgoogle.com";

function truncateToDays(aMsec) {
  return Math.floor(aMsec / MILLISECONDS_PER_DAY);
}

var SysInfo = {
  overrides: {},

  getProperty(name) {
    // Assert.ok(false, "Mock SysInfo: " + name + ", " + JSON.stringify(this.overrides));
    if (name in this.overrides) {
      return this.overrides[name];
    }

    return this._genuine.QueryInterface(Ci.nsIPropertyBag).getProperty(name);
  },

  getPropertyAsACString(name) {
    return this.get(name);
  },

  getPropertyAsUint32(name) {
    return this.get(name);
  },

  get(name) {
    return this._genuine.QueryInterface(Ci.nsIPropertyBag2).get(name);
  },

  get diskInfo() {
    return this._genuine.QueryInterface(Ci.nsISystemInfo).diskInfo;
  },

  get osInfo() {
    return this._genuine.QueryInterface(Ci.nsISystemInfo).osInfo;
  },

  get processInfo() {
    return this._genuine.QueryInterface(Ci.nsISystemInfo).processInfo;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPropertyBag2", "nsISystemInfo"]),
};

/**
 * TelemetryEnvironmentTesting - tools for testing the telemetry environment
 * reporting.
 */
var TelemetryEnvironmentTesting = {
  EXPECTED_HDD_FIELDS,

  init(appInfo) {
    this.appInfo = appInfo;
  },

  setSysInfoOverrides(overrides) {
    SysInfo.overrides = overrides;
  },

  spoofGfxAdapter() {
    try {
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(
        Ci.nsIGfxInfoDebug
      );
      gfxInfo.fireTestProcess();
      gfxInfo.spoofVendorID(GFX_VENDOR_ID);
      gfxInfo.spoofDeviceID(GFX_DEVICE_ID);
    } catch (x) {
      // If we can't test gfxInfo, that's fine, we'll note it later.
    }
  },

  spoofProfileReset() {
    return CommonUtils.writeJSON(
      {
        created: PROFILE_CREATION_DATE_MS,
        reset: PROFILE_RESET_DATE_MS,
        firstUse: PROFILE_FIRST_USE_MS,
      },
      OS.Path.join(OS.Constants.Path.profileDir, "times.json")
    );
  },

  spoofPartnerInfo() {
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
  },

  async spoofAttributionData() {
    if (gIsWindows || gIsMac) {
      AttributionCode._clearCache();
      await AttributionCode.writeAttributionFile(ATTRIBUTION_CODE);
    }
  },

  cleanupAttributionData() {
    if (gIsWindows || gIsMac) {
      AttributionCode.attributionFile.remove(false);
      AttributionCode._clearCache();
    }
  },

  registerFakeSysInfo() {
    MockRegistrar.register("@mozilla.org/system-info;1", SysInfo);
  },

  /**
   * Check that a value is a string and not empty.
   *
   * @param aValue The variable to check.
   * @return True if |aValue| has type "string" and is not empty, False otherwise.
   */
  checkString(aValue) {
    return typeof aValue == "string" && aValue != "";
  },

  /**
   * If value is non-null, check if it's a valid string.
   *
   * @param aValue The variable to check.
   * @return True if it's null or a valid string, false if it's non-null and an invalid
   *         string.
   */
  checkNullOrString(aValue) {
    if (aValue) {
      return this.checkString(aValue);
    } else if (aValue === null) {
      return true;
    }

    return false;
  },

  /**
   * If value is non-null, check if it's a boolean.
   *
   * @param aValue The variable to check.
   * @return True if it's null or a valid boolean, false if it's non-null and an invalid
   *         boolean.
   */
  checkNullOrBool(aValue) {
    return aValue === null || typeof aValue == "boolean";
  },

  checkBuildSection(data) {
    const expectedInfo = {
      applicationId: APP_ID,
      applicationName: APP_NAME,
      buildId: this.appInfo.appBuildID,
      version: APP_VERSION,
      vendor: "Mozilla",
      platformVersion: PLATFORM_VERSION,
      xpcomAbi: "noarch-spidermonkey",
    };

    Assert.ok("build" in data, "There must be a build section in Environment.");

    for (let f in expectedInfo) {
      Assert.ok(
        this.checkString(data.build[f]),
        f + " must be a valid string."
      );
      Assert.equal(
        data.build[f],
        expectedInfo[f],
        f + " must have the correct value."
      );
    }

    // Make sure architecture is in the environment.
    Assert.ok(this.checkString(data.build.architecture));

    Assert.equal(
      data.build.updaterAvailable,
      AppConstants.MOZ_UPDATER,
      "build.updaterAvailable must equal AppConstants.MOZ_UPDATER"
    );
  },

  checkSettingsSection(data) {
    const EXPECTED_FIELDS_TYPES = {
      blocklistEnabled: "boolean",
      e10sEnabled: "boolean",
      e10sMultiProcesses: "number",
      fissionEnabled: "boolean",
      intl: "object",
      locale: "string",
      telemetryEnabled: "boolean",
      update: "object",
      userPrefs: "object",
    };

    Assert.ok(
      "settings" in data,
      "There must be a settings section in Environment."
    );

    for (let f in EXPECTED_FIELDS_TYPES) {
      Assert.equal(
        typeof data.settings[f],
        EXPECTED_FIELDS_TYPES[f],
        f + " must have the correct type."
      );
    }

    // This property is not always present, but when it is, it must be a number.
    if ("launcherProcessState" in data.settings) {
      Assert.equal(typeof data.settings.launcherProcessState, "number");
    }

    // Check "addonCompatibilityCheckEnabled" separately.
    Assert.equal(
      data.settings.addonCompatibilityCheckEnabled,
      AddonManager.checkCompatibility
    );

    // Check "isDefaultBrowser" separately, as it is not available on Android an can either be
    // null or boolean on other platforms.
    if (gIsAndroid) {
      Assert.ok(
        !("isDefaultBrowser" in data.settings),
        "Must not be available on Android."
      );
    } else if ("isDefaultBrowser" in data.settings) {
      // isDefaultBrowser might not be available in the payload, since it's
      // gathered after the session was restored.
      Assert.ok(this.checkNullOrBool(data.settings.isDefaultBrowser));
    }

    // Check "channel" separately, as it can either be null or string.
    let update = data.settings.update;
    Assert.ok(this.checkNullOrString(update.channel));
    Assert.equal(typeof update.enabled, "boolean");
    Assert.equal(typeof update.autoDownload, "boolean");
    Assert.equal(typeof update.background, "boolean");

    // Check "defaultSearchEngine" separately, as it can either be undefined or string.
    if ("defaultSearchEngine" in data.settings) {
      this.checkString(data.settings.defaultSearchEngine);
      Assert.equal(typeof data.settings.defaultSearchEngineData, "object");
    }

    if ("defaultPrivateSearchEngineData" in data.settings) {
      Assert.equal(
        typeof data.settings.defaultPrivateSearchEngineData,
        "object"
      );
    }

    if ((gIsWindows || gIsMac) && AppConstants.MOZ_BUILD_APP == "browser") {
      Assert.equal(typeof data.settings.attribution, "object");
      Assert.equal(data.settings.attribution.source, "google.com");
    }

    this.checkIntlSettings(data.settings);
  },

  checkIntlSettings({ intl }) {
    let fields = [
      "requestedLocales",
      "availableLocales",
      "appLocales",
      "acceptLanguages",
    ];

    for (let field of fields) {
      Assert.ok(Array.isArray(intl[field]), `${field} is an array`);
    }

    // These fields may be null if they aren't ready yet. This is mostly to deal
    // with test failures on Android, but they aren't guaranteed to exist.
    let optionalFields = ["systemLocales", "regionalPrefsLocales"];

    for (let field of optionalFields) {
      let isArray = Array.isArray(intl[field]);
      let isNull = intl[field] === null;
      Assert.ok(isArray || isNull, `${field} is an array or null`);
    }
  },

  checkProfileSection(data) {
    Assert.ok(
      "profile" in data,
      "There must be a profile section in Environment."
    );
    Assert.equal(
      data.profile.creationDate,
      truncateToDays(PROFILE_CREATION_DATE_MS)
    );
    Assert.equal(data.profile.resetDate, truncateToDays(PROFILE_RESET_DATE_MS));
    Assert.equal(
      data.profile.firstUseDate,
      truncateToDays(PROFILE_FIRST_USE_MS)
    );
  },

  checkPartnerSection(data, isInitial) {
    const EXPECTED_FIELDS = {
      distributionId: DISTRIBUTION_ID,
      distributionVersion: DISTRIBUTION_VERSION,
      partnerId: PARTNER_ID,
      distributor: DISTRIBUTOR_NAME,
      distributorChannel: DISTRIBUTOR_CHANNEL,
    };

    Assert.ok(
      "partner" in data,
      "There must be a partner section in Environment."
    );

    for (let f in EXPECTED_FIELDS) {
      let expected = isInitial ? null : EXPECTED_FIELDS[f];
      Assert.strictEqual(
        data.partner[f],
        expected,
        f + " must have the correct value."
      );
    }

    // Check that "partnerNames" exists and contains the correct element.
    Assert.ok(Array.isArray(data.partner.partnerNames));
    if (isInitial) {
      Assert.equal(data.partner.partnerNames.length, 0);
    } else {
      Assert.ok(data.partner.partnerNames.includes(PARTNER_NAME));
    }
  },

  checkGfxAdapter(data) {
    const EXPECTED_ADAPTER_FIELDS_TYPES = {
      description: "string",
      vendorID: "string",
      deviceID: "string",
      subsysID: "string",
      RAM: "number",
      driver: "string",
      driverVendor: "string",
      driverVersion: "string",
      driverDate: "string",
      GPUActive: "boolean",
    };

    for (let f in EXPECTED_ADAPTER_FIELDS_TYPES) {
      Assert.ok(f in data, f + " must be available.");

      if (data[f]) {
        // Since we have a non-null value, check if it has the correct type.
        Assert.equal(
          typeof data[f],
          EXPECTED_ADAPTER_FIELDS_TYPES[f],
          f + " must have the correct type."
        );
      }
    }
  },

  checkSystemSection(data, assertProcessData) {
    const EXPECTED_FIELDS = [
      "memoryMB",
      "cpu",
      "os",
      "hdd",
      "gfx",
      "appleModelId",
    ];

    Assert.ok(
      "system" in data,
      "There must be a system section in Environment."
    );

    // Make sure we have all the top level sections and fields.
    for (let f of EXPECTED_FIELDS) {
      Assert.ok(f in data.system, f + " must be available.");
    }

    Assert.ok(
      Number.isFinite(data.system.memoryMB),
      "MemoryMB must be a number."
    );

    if (assertProcessData) {
      if (gIsWindows || gIsMac || gIsLinux) {
        let EXTRA_CPU_FIELDS = [
          "cores",
          "model",
          "family",
          "stepping",
          "l2cacheKB",
          "l3cacheKB",
          "speedMHz",
          "vendor",
        ];

        for (let f of EXTRA_CPU_FIELDS) {
          // Note this is testing TelemetryEnvironment.js only, not that the
          // values are valid - null is the fallback.
          Assert.ok(f in data.system.cpu, f + " must be available under cpu.");
        }

        if (gIsWindows) {
          Assert.equal(
            typeof data.system.isWow64,
            "boolean",
            "isWow64 must be available on Windows and have the correct type."
          );
          Assert.equal(
            typeof data.system.isWowARM64,
            "boolean",
            "isWowARM64 must be available on Windows and have the correct type."
          );
          Assert.ok(
            "virtualMaxMB" in data.system,
            "virtualMaxMB must be available."
          );
          Assert.ok(
            Number.isFinite(data.system.virtualMaxMB),
            "virtualMaxMB must be a number."
          );

          for (let f of [
            "count",
            "model",
            "family",
            "stepping",
            "l2cacheKB",
            "l3cacheKB",
            "speedMHz",
          ]) {
            Assert.ok(
              Number.isFinite(data.system.cpu[f]),
              f + " must be a number if non null."
            );
          }
        }

        // These should be numbers if they are not null
        for (let f of [
          "count",
          "model",
          "family",
          "stepping",
          "l2cacheKB",
          "l3cacheKB",
          "speedMHz",
        ]) {
          Assert.ok(
            !(f in data.system.cpu) ||
              data.system.cpu[f] === null ||
              Number.isFinite(data.system.cpu[f]),
            f + " must be a number if non null."
          );
        }

        // We insist these are available
        for (let f of ["cores"]) {
          Assert.ok(
            !(f in data.system.cpu) || Number.isFinite(data.system.cpu[f]),
            f + " must be a number if non null."
          );
        }
      }
    }

    let cpuData = data.system.cpu;

    Assert.ok(
      Array.isArray(cpuData.extensions),
      "CPU extensions must be available."
    );

    let osData = data.system.os;
    Assert.ok(this.checkNullOrString(osData.name));
    Assert.ok(this.checkNullOrString(osData.version));
    Assert.ok(this.checkNullOrString(osData.locale));

    // Service pack is only available on Windows.
    if (gIsWindows) {
      Assert.ok(
        Number.isFinite(osData.servicePackMajor),
        "ServicePackMajor must be a number."
      );
      Assert.ok(
        Number.isFinite(osData.servicePackMinor),
        "ServicePackMinor must be a number."
      );
      if ("windowsBuildNumber" in osData) {
        // This might not be available on all Windows platforms.
        Assert.ok(
          Number.isFinite(osData.windowsBuildNumber),
          "windowsBuildNumber must be a number."
        );
      }
      if ("windowsUBR" in osData) {
        // This might not be available on all Windows platforms.
        Assert.ok(
          osData.windowsUBR === null || Number.isFinite(osData.windowsUBR),
          "windowsUBR must be null or a number."
        );
      }
    } else if (gIsAndroid) {
      Assert.ok(this.checkNullOrString(osData.kernelVersion));
    }

    for (let disk of EXPECTED_HDD_FIELDS) {
      Assert.ok(this.checkNullOrString(data.system.hdd[disk].model));
      Assert.ok(this.checkNullOrString(data.system.hdd[disk].revision));
      Assert.ok(this.checkNullOrString(data.system.hdd[disk].type));
    }

    let gfxData = data.system.gfx;
    Assert.ok("D2DEnabled" in gfxData);
    Assert.ok("DWriteEnabled" in gfxData);
    Assert.ok("Headless" in gfxData);
    Assert.ok("EmbeddedInFirefoxReality" in gfxData);
    // DWriteVersion is disabled due to main thread jank and will be enabled
    // again as part of bug 1154500.
    // Assert.ok("DWriteVersion" in gfxData);
    if (gIsWindows) {
      Assert.equal(typeof gfxData.D2DEnabled, "boolean");
      Assert.equal(typeof gfxData.DWriteEnabled, "boolean");
      Assert.equal(typeof gfxData.EmbeddedInFirefoxReality, "boolean");
      // As above, will be enabled again as part of bug 1154500.
      // Assert.ok(this.checkString(gfxData.DWriteVersion));
    }

    Assert.ok("adapters" in gfxData);
    Assert.ok(
      !!gfxData.adapters.length,
      "There must be at least one GFX adapter."
    );
    for (let adapter of gfxData.adapters) {
      this.checkGfxAdapter(adapter);
    }
    Assert.equal(typeof gfxData.adapters[0].GPUActive, "boolean");
    Assert.ok(
      gfxData.adapters[0].GPUActive,
      "The first GFX adapter must be active."
    );

    Assert.ok(Array.isArray(gfxData.monitors));
    if (gIsWindows || gIsMac || gIsLinux) {
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

    Assert.equal(typeof gfxData.features.gpuProcess, "object");
    Assert.equal(typeof gfxData.features.gpuProcess.status, "string");

    try {
      // If we've not got nsIGfxInfoDebug, then this will throw and stop us doing
      // this test.
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(
        Ci.nsIGfxInfoDebug
      );

      if (gIsWindows || gIsMac) {
        Assert.equal(GFX_VENDOR_ID, gfxData.adapters[0].vendorID);
        Assert.equal(GFX_DEVICE_ID, gfxData.adapters[0].deviceID);
      }

      let features = gfxInfo.getFeatures();
      Assert.equal(features.compositor, gfxData.features.compositor);
      Assert.equal(
        features.gpuProcess.status,
        gfxData.features.gpuProcess.status
      );
      Assert.equal(features.opengl, gfxData.features.opengl);
      Assert.equal(features.webgl, gfxData.features.webgl);
    } catch (e) {}

    if (gIsMac) {
      Assert.ok(this.checkString(data.system.appleModelId));
    } else {
      Assert.ok(this.checkNullOrString(data.system.appleModelId));
    }

    // This feature is only available on Windows 8+
    if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
      Assert.ok(
        "sec" in data.system,
        "sec must be available under data.system"
      );

      let SEC_FIELDS = ["antivirus", "antispyware", "firewall"];
      for (let f of SEC_FIELDS) {
        Assert.ok(
          f in data.system.sec,
          f + " must be available under data.system.sec"
        );

        let value = data.system.sec[f];
        // value is null on Windows Server
        Assert.ok(
          value === null || Array.isArray(value),
          f + " must be either null or an array"
        );
        if (Array.isArray(value)) {
          for (let product of value) {
            Assert.equal(
              typeof product,
              "string",
              "Each element of " + f + " must be a string"
            );
          }
        }
      }
    }
  },

  checkActiveAddon(data, partialRecord) {
    let signedState = "number";
    // system add-ons have an undefined signState
    if (data.isSystem) {
      signedState = "undefined";
    }

    const EXPECTED_ADDON_FIELDS_TYPES = {
      version: "string",
      scope: "number",
      type: "string",
      updateDay: "number",
      isSystem: "boolean",
      isWebExtension: "boolean",
      multiprocessCompatible: "boolean",
    };

    const FULL_ADDON_FIELD_TYPES = {
      blocklisted: "boolean",
      name: "string",
      userDisabled: "boolean",
      appDisabled: "boolean",
      foreignInstall: "boolean",
      hasBinaryComponents: "boolean",
      installDay: "number",
      signedState,
    };

    let fields = EXPECTED_ADDON_FIELDS_TYPES;
    if (!partialRecord) {
      fields = Object.assign({}, fields, FULL_ADDON_FIELD_TYPES);
    }

    for (let [name, type] of Object.entries(fields)) {
      Assert.ok(name in data, name + " must be available.");
      Assert.equal(
        typeof data[name],
        type,
        name + " must have the correct type."
      );
    }

    if (!partialRecord) {
      // We check "description" separately, as it can be null.
      Assert.ok(this.checkNullOrString(data.description));
    }
  },

  checkTheme(data) {
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
      Assert.equal(
        typeof data[f],
        EXPECTED_THEME_FIELDS_TYPES[f],
        f + " must have the correct type."
      );
    }

    // We check "description" separately, as it can be null.
    Assert.ok(this.checkNullOrString(data.description));
  },

  checkActiveGMPlugin(data) {
    // GMP plugin version defaults to null until GMPDownloader runs to update it.
    if (data.version) {
      Assert.equal(typeof data.version, "string");
    }
    Assert.equal(typeof data.userDisabled, "boolean");
    Assert.equal(typeof data.applyBackgroundUpdates, "number");
  },

  checkAddonsSection(data, expectBrokenAddons, partialAddonsRecords) {
    const EXPECTED_FIELDS = ["activeAddons", "theme", "activeGMPlugins"];

    Assert.ok(
      "addons" in data,
      "There must be an addons section in Environment."
    );
    for (let f of EXPECTED_FIELDS) {
      Assert.ok(f in data.addons, f + " must be available.");
    }

    // Check the active addons, if available.
    if (!expectBrokenAddons) {
      let activeAddons = data.addons.activeAddons;
      for (let addon in activeAddons) {
        this.checkActiveAddon(activeAddons[addon], partialAddonsRecords);
      }
    }

    // Check "theme" structure.
    if (Object.keys(data.addons.theme).length !== 0) {
      this.checkTheme(data.addons.theme);
    }

    // Check active GMPlugins
    let activeGMPlugins = data.addons.activeGMPlugins;
    for (let gmPlugin in activeGMPlugins) {
      this.checkActiveGMPlugin(activeGMPlugins[gmPlugin]);
    }
  },

  checkEnvironmentData(data, options = {}) {
    const {
      isInitial = false,
      expectBrokenAddons = false,
      assertProcessData = false,
    } = options;

    this.checkBuildSection(data);
    this.checkSettingsSection(data);
    this.checkProfileSection(data);
    this.checkPartnerSection(data, isInitial);
    this.checkSystemSection(data, assertProcessData);
    this.checkAddonsSection(data, expectBrokenAddons);
  },
};
