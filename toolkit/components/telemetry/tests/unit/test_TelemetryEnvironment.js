/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonManager, AddonManagerPrivate } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", this);
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

// AttributionCode is only needed for Firefox
ChromeUtils.defineModuleGetter(
  this,
  "AttributionCode",
  "resource:///modules/AttributionCode.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionTestUtils",
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

async function installXPIFromURL(url) {
  let install = await AddonManager.getInstallForURL(url);
  return install.install();
}

function promiseNextTick() {
  return new Promise(resolve => executeSoon(resolve));
}

// The webserver hosting the addons.
var gHttpServer = null;
// The URL of the webserver root.
var gHttpRoot = null;
// The URL of the data directory, on the webserver.
var gDataRoot = null;

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
const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";

const GFX_VENDOR_ID = "0xabcd";
const GFX_DEVICE_ID = "0x1234";

// The profile reset date, in milliseconds (Today)
const PROFILE_RESET_DATE_MS = Date.now();
// The profile creation date, in milliseconds (Yesterday).
const PROFILE_FIRST_USE_MS = PROFILE_RESET_DATE_MS - MILLISECONDS_PER_DAY;
const PROFILE_CREATION_DATE_MS = PROFILE_FIRST_USE_MS - MILLISECONDS_PER_DAY;

const FLASH_PLUGIN_NAME = "Shockwave Flash";
const FLASH_PLUGIN_DESC = "A mock flash plugin";
const FLASH_PLUGIN_VERSION = "\u201c1.1.1.1\u201d";
const PLUGIN_MIME_TYPE1 = "application/x-shockwave-flash";
const PLUGIN_MIME_TYPE2 = "text/plain";

const PLUGIN2_NAME = "Quicktime";
const PLUGIN2_DESC = "A mock Quicktime plugin";
const PLUGIN2_VERSION = "2.3";

const PLUGIN_UPDATED_TOPIC = "plugins-list-updated";

// system add-ons are enabled at startup, so record date when the test starts
const SYSTEM_ADDON_INSTALL_DATE = Date.now();

const EXPECTED_HDD_FIELDS = ["profile", "binary", "system"];

// Valid attribution code to write so that settings.attribution can be tested.
const ATTRIBUTION_CODE = "source%3Dgoogle.com";

const pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(
  Ci.nsIPluginHost
);

/**
 * Used to mock plugin tags in our fake plugin host.
 */
function PluginTag(aName, aDescription, aVersion, aEnabled) {
  this.pluginTag = pluginHost.createFakePlugin({
    handlerURI: "resource://fake-plugin/${Math.random()}.xhtml",
    mimeEntries: this.mimeTypes.map(type => ({ type })),
    name: aName,
    description: aDescription,
    fileName: `${aName}.so`,
    version: aVersion,
  });
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
  blocklisted: false,
  clicktoplay: true,

  get disabled() {
    return this.pluginTag.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
  set disabled(val) {
    this.pluginTag.enabledState =
      Ci.nsIPluginTag[val ? "STATE_DISABLED" : "STATE_CLICKTOPLAY"];
  },

  mimeTypes: [PLUGIN_MIME_TYPE1, PLUGIN_MIME_TYPE2],

  getMimeTypes() {
    return this.mimeTypes;
  },
};

// A container for the plugins handled by the fake plugin host.
var gInstalledPlugins = [
  new PluginTag("Java", "A mock Java plugin", "1.0", false /* Disabled */),
  new PluginTag(
    FLASH_PLUGIN_NAME,
    FLASH_PLUGIN_DESC,
    FLASH_PLUGIN_VERSION,
    true
  ),
];

// A fake plugin host for testing plugin telemetry environment.
var PluginHost = {
  getPluginTags() {
    return gInstalledPlugins.map(plugin => plugin.pluginTag);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPluginHost"]),
};

function registerFakePluginHost() {
  MockRegistrar.register("@mozilla.org/plugin/host;1", PluginHost);
}

var SysInfo = {
  overrides: {},

  getProperty(name) {
    // Assert.ok(false, "Mock SysInfo: " + name + ", " + JSON.stringify(this.overrides));
    if (name in this.overrides) {
      return this.overrides[name];
    }

    return this._genuine.getProperty(name);
  },

  getPropertyAsUint32(name) {
    return this.get(name);
  },

  get(name) {
    return this._genuine.get(name);
  },

  get diskInfo() {
    return this._genuine.QueryInterface(Ci.nsISystemInfo).diskInfo;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPropertyBag2", "nsISystemInfo"]),
};

function registerFakeSysInfo() {
  MockRegistrar.register("@mozilla.org/system-info;1", SysInfo);
}

function MockAddonWrapper(aAddon) {
  this.addon = aAddon;
}
MockAddonWrapper.prototype = {
  get id() {
    return this.addon.id;
  },

  get type() {
    return "service";
  },

  get appDisabled() {
    return false;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get scope() {
    return AddonManager.SCOPE_PROFILE;
  },

  get foreignInstall() {
    return false;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get blocklistState() {
    return 0; // Not blocked.
  },

  get pendingOperations() {
    return AddonManager.PENDING_NONE;
  },

  get permissions() {
    return AddonManager.PERM_CAN_UNINSTALL | AddonManager.PERM_CAN_DISABLE;
  },

  get isActive() {
    return true;
  },

  get name() {
    return this.addon.name;
  },

  get version() {
    return this.addon.version;
  },

  get creator() {
    return new AddonManagerPrivate.AddonAuthor(this.addon.author);
  },

  get userDisabled() {
    return this.appDisabled;
  },
};

function createMockAddonProvider(aName) {
  let mockProvider = {
    _addons: [],

    get name() {
      return aName;
    },

    addAddon(aAddon) {
      this._addons.push(aAddon);
      AddonManagerPrivate.callAddonListeners(
        "onInstalled",
        new MockAddonWrapper(aAddon)
      );
    },

    async getAddonsByTypes(aTypes) {
      return this._addons
        .filter(a => !aTypes || aTypes.includes(a.type))
        .map(a => new MockAddonWrapper(a));
    },

    shutdown() {
      return Promise.resolve();
    },
  };

  return mockProvider;
}

function spoofGfxAdapter() {
  try {
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);
    gfxInfo.fireTestProcess();
    gfxInfo.spoofVendorID(GFX_VENDOR_ID);
    gfxInfo.spoofDeviceID(GFX_DEVICE_ID);
  } catch (x) {
    // If we can't test gfxInfo, that's fine, we'll note it later.
  }
}

function spoofProfileReset() {
  return CommonUtils.writeJSON(
    {
      created: PROFILE_CREATION_DATE_MS,
      reset: PROFILE_RESET_DATE_MS,
      firstUse: PROFILE_FIRST_USE_MS,
    },
    OS.Path.join(OS.Constants.Path.profileDir, "times.json")
  );
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

function getAttributionFile() {
  return FileUtils.getFile("LocalAppData", [
    "mozilla",
    AppConstants.MOZ_APP_NAME,
    "postSigningData",
  ]);
}

function spoofAttributionData() {
  if (gIsWindows) {
    AttributionCode._clearCache();
    let stream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
      Ci.nsIFileOutputStream
    );
    stream.init(getAttributionFile(), -1, -1, 0);
    stream.write(ATTRIBUTION_CODE, ATTRIBUTION_CODE.length);
    stream.close();
  }
}

function cleanupAttributionData() {
  if (gIsWindows) {
    getAttributionFile().remove(false);
    AttributionCode._clearCache();
  }
}

/**
 * Check that a value is a string and not empty.
 *
 * @param aValue The variable to check.
 * @return True if |aValue| has type "string" and is not empty, False otherwise.
 */
function checkString(aValue) {
  return typeof aValue == "string" && aValue != "";
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
  return aValue === null || typeof aValue == "boolean";
}

function checkBuildSection(data) {
  const expectedInfo = {
    applicationId: APP_ID,
    applicationName: APP_NAME,
    buildId: gAppInfo.appBuildID,
    version: APP_VERSION,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  Assert.ok("build" in data, "There must be a build section in Environment.");

  for (let f in expectedInfo) {
    Assert.ok(checkString(data.build[f]), f + " must be a valid string.");
    Assert.equal(
      data.build[f],
      expectedInfo[f],
      f + " must have the correct value."
    );
  }

  // Make sure architecture is in the environment.
  Assert.ok(checkString(data.build.architecture));

  Assert.equal(
    data.build.updaterAvailable,
    AppConstants.MOZ_UPDATER,
    "build.updaterAvailable must equal AppConstants.MOZ_UPDATER"
  );
}

function checkSettingsSection(data) {
  const EXPECTED_FIELDS_TYPES = {
    blocklistEnabled: "boolean",
    e10sEnabled: "boolean",
    e10sMultiProcesses: "number",
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

  if (gIsWindows && AppConstants.MOZ_BUILD_APP == "browser") {
    Assert.equal(typeof data.settings.attribution, "object");
    Assert.equal(data.settings.attribution.source, "google.com");
  }

  checkIntlSettings(data.settings);
}

function checkIntlSettings({ intl }) {
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
}

function checkProfileSection(data) {
  Assert.ok(
    "profile" in data,
    "There must be a profile section in Environment."
  );
  Assert.equal(
    data.profile.creationDate,
    truncateToDays(PROFILE_CREATION_DATE_MS)
  );
  Assert.equal(data.profile.resetDate, truncateToDays(PROFILE_RESET_DATE_MS));
  Assert.equal(data.profile.firstUseDate, truncateToDays(PROFILE_FIRST_USE_MS));
}

function checkPartnerSection(data, isInitial) {
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
}

function checkGfxAdapter(data) {
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
}

function checkSystemSection(data) {
  const EXPECTED_FIELDS = [
    "memoryMB",
    "cpu",
    "os",
    "hdd",
    "gfx",
    "appleModelId",
  ];

  Assert.ok("system" in data, "There must be a system section in Environment.");

  // Make sure we have all the top level sections and fields.
  for (let f of EXPECTED_FIELDS) {
    Assert.ok(f in data.system, f + " must be available.");
  }

  Assert.ok(
    Number.isFinite(data.system.memoryMB),
    "MemoryMB must be a number."
  );

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
    }

    // We insist these are available
    for (let f of ["cores"]) {
      Assert.ok(
        !(f in data.system.cpu) || Number.isFinite(data.system.cpu[f]),
        f + " must be a number if non null."
      );
    }

    // These should be numbers if they are not null
    for (let f of [
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
  }

  let cpuData = data.system.cpu;
  Assert.ok(Number.isFinite(cpuData.count), "CPU count must be a number.");
  Assert.ok(
    Array.isArray(cpuData.extensions),
    "CPU extensions must be available."
  );

  let osData = data.system.os;
  Assert.ok(checkNullOrString(osData.name));
  Assert.ok(checkNullOrString(osData.version));
  Assert.ok(checkNullOrString(osData.locale));

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
    Assert.ok(checkNullOrString(osData.kernelVersion));
  }

  for (let disk of EXPECTED_HDD_FIELDS) {
    Assert.ok(checkNullOrString(data.system.hdd[disk].model));
    Assert.ok(checkNullOrString(data.system.hdd[disk].revision));
    Assert.ok(checkNullOrString(data.system.hdd[disk].type));
  }

  let gfxData = data.system.gfx;
  Assert.ok("D2DEnabled" in gfxData);
  Assert.ok("DWriteEnabled" in gfxData);
  Assert.ok("Headless" in gfxData);
  // DWriteVersion is disabled due to main thread jank and will be enabled
  // again as part of bug 1154500.
  // Assert.ok("DWriteVersion" in gfxData);
  if (gIsWindows) {
    Assert.equal(typeof gfxData.D2DEnabled, "boolean");
    Assert.equal(typeof gfxData.DWriteEnabled, "boolean");
    // As above, will be enabled again as part of bug 1154500.
    // Assert.ok(checkString(gfxData.DWriteVersion));
  }

  Assert.ok("adapters" in gfxData);
  Assert.ok(
    gfxData.adapters.length > 0,
    "There must be at least one GFX adapter."
  );
  for (let adapter of gfxData.adapters) {
    checkGfxAdapter(adapter);
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
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfoDebug);

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
    Assert.ok(checkString(data.system.appleModelId));
  } else {
    Assert.ok(checkNullOrString(data.system.appleModelId));
  }

  // This feature is only available on Windows 8+
  if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
    Assert.ok("sec" in data.system, "sec must be available under data.system");

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
}

function checkActiveAddon(data, partialRecord) {
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
    Assert.ok(checkNullOrString(data.description));
  }
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
    Assert.equal(
      typeof data[f],
      EXPECTED_PLUGIN_FIELDS_TYPES[f],
      f + " must have the correct type."
    );
  }

  Assert.ok(Array.isArray(data.mimeTypes));
  for (let type of data.mimeTypes) {
    Assert.ok(checkString(type));
  }
}

function checkTheme(data) {
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
  Assert.ok(checkNullOrString(data.description));
}

function checkActiveGMPlugin(data) {
  // GMP plugin version defaults to null until GMPDownloader runs to update it.
  if (data.version) {
    Assert.equal(typeof data.version, "string");
  }
  Assert.equal(typeof data.userDisabled, "boolean");
  Assert.equal(typeof data.applyBackgroundUpdates, "number");
}

function checkAddonsSection(data, expectBrokenAddons, partialAddonsRecords) {
  const EXPECTED_FIELDS = [
    "activeAddons",
    "theme",
    "activePlugins",
    "activeGMPlugins",
  ];

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
      checkActiveAddon(activeAddons[addon], partialAddonsRecords);
    }
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
  for (let gmPlugin in activeGMPlugins) {
    checkActiveGMPlugin(activeGMPlugins[gmPlugin]);
  }
}

function checkExperimentsSection(data) {
  // We don't expect the experiments section to be always available.
  let experiments = data.experiments || {};
  if (Object.keys(experiments).length == 0) {
    return;
  }

  for (let id in experiments) {
    Assert.ok(checkString(id), id + " must be a valid string.");

    // Check that we have valid experiment info.
    let experimentData = experiments[id];
    Assert.ok(
      "branch" in experimentData,
      "The experiment must have branch data."
    );
    Assert.ok(
      checkString(experimentData.branch),
      "The experiment data must be valid."
    );
    if ("type" in experimentData) {
      Assert.ok(checkString(experimentData.type));
    }
  }
}

function checkEnvironmentData(data, options = {}) {
  const { isInitial = false, expectBrokenAddons = false } = options;

  checkBuildSection(data);
  checkSettingsSection(data);
  checkProfileSection(data);
  checkPartnerSection(data, isInitial);
  checkSystemSection(data);
  checkAddonsSection(data, expectBrokenAddons);
}

add_task(async function setup() {
  registerFakeSysInfo();
  spoofGfxAdapter();
  do_get_profile();

  // The system add-on must be installed before AddonManager is started.
  const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
  do_get_file("system.xpi").copyTo(
    distroDir,
    "tel-system-xpi@tests.mozilla.org.xpi"
  );
  let system_addon = FileUtils.File(distroDir.path);
  system_addon.append("tel-system-xpi@tests.mozilla.org.xpi");
  system_addon.lastModifiedTime = SYSTEM_ADDON_INSTALL_DATE;
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);

  // The test runs in a fresh profile so starting the AddonManager causes
  // the addons database to be created (as does setting new theme).
  // For test_addonsStartup below, we want to test a "warm" startup where
  // there is already a database on disk.  Simulate that here by just
  // restarting the AddonManager.
  await AddonTestUtils.promiseShutdownManager();
  await AddonTestUtils.overrideBuiltIns({ system: [] });
  AddonTestUtils.addonStartup.remove(true);
  await AddonTestUtils.promiseStartupManager();

  // Register a fake plugin host for consistent flash version data.
  registerFakePluginHost();

  // Setup a webserver to serve Addons, Plugins, etc.
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
  let port = gHttpServer.identity.primaryPort;
  gHttpRoot = "http://localhost:" + port + "/";
  gDataRoot = gHttpRoot + "data/";
  gHttpServer.registerDirectory("/data/", do_get_cwd());
  registerCleanupFunction(() => gHttpServer.stop(() => {}));

  // Create the attribution data file, so that settings.attribution will exist.
  // The attribution functionality only exists in Firefox.
  if (AppConstants.MOZ_BUILD_APP == "browser") {
    spoofAttributionData();
    registerCleanupFunction(cleanupAttributionData);
  }

  await spoofProfileReset();
  TelemetryEnvironment.delayedInit();
});

add_task(async function test_checkEnvironment() {
  // During startup we have partial addon records.
  // First make sure we haven't yet read the addons DB, then test that
  // we have some partial addons data.
  Assert.equal(
    AddonManagerPrivate.isDBLoaded(),
    false,
    "addons database is not loaded"
  );

  let data = TelemetryEnvironment.currentEnvironment;
  checkAddonsSection(data, false, true);

  // Check that settings.intl is lazily loaded.
  Assert.equal(
    typeof data.settings.intl,
    "object",
    "intl is initially an object"
  );
  Assert.equal(
    Object.keys(data.settings.intl).length,
    0,
    "intl is initially empty"
  );

  // Now continue with startup.
  let initPromise = TelemetryEnvironment.onInitialized();
  finishAddonManagerStartup();

  // Fake the delayed startup event for intl data to load.
  fakeIntlReady();

  let environmentData = await initPromise;
  checkEnvironmentData(environmentData, { isInitial: true });

  spoofPartnerInfo();
  Services.obs.notifyObservers(null, DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC);

  environmentData = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(environmentData);
});

add_task(async function test_prefWatchPolicies() {
  const PREF_TEST_1 = "toolkit.telemetry.test.pref_new";
  const PREF_TEST_2 = "toolkit.telemetry.test.pref1";
  const PREF_TEST_3 = "toolkit.telemetry.test.pref2";
  const PREF_TEST_4 = "toolkit.telemetry.test.pref_old";
  const PREF_TEST_5 = "toolkit.telemetry.test.requiresRestart";

  const expectedValue = "some-test-value";
  const unexpectedValue = "unexpected-test-value";

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST_1, { what: TelemetryEnvironment.RECORD_PREF_VALUE }],
    [PREF_TEST_2, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
    [PREF_TEST_3, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
    [PREF_TEST_4, { what: TelemetryEnvironment.RECORD_PREF_VALUE }],
    [
      PREF_TEST_5,
      { what: TelemetryEnvironment.RECORD_PREF_VALUE, requiresRestart: true },
    ],
  ]);

  Preferences.set(PREF_TEST_4, expectedValue);
  Preferences.set(PREF_TEST_5, expectedValue);

  // Set the Environment preferences to watch.
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  let deferred = PromiseUtils.defer();

  // Check that the pref values are missing or present as expected
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_1],
    undefined
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_4],
    expectedValue
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_5],
    expectedValue
  );

  TelemetryEnvironment.registerChangeListener(
    "testWatchPrefs",
    (reason, data) => deferred.resolve(data)
  );
  let oldEnvironmentData = TelemetryEnvironment.currentEnvironment;

  // Trigger a change in the watched preferences.
  Preferences.set(PREF_TEST_1, expectedValue);
  Preferences.set(PREF_TEST_2, false);
  Preferences.set(PREF_TEST_5, unexpectedValue);
  let eventEnvironmentData = await deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs");

  // Check environment contains the correct data.
  Assert.deepEqual(oldEnvironmentData, eventEnvironmentData);
  let userPrefs = TelemetryEnvironment.currentEnvironment.settings.userPrefs;

  Assert.equal(
    userPrefs[PREF_TEST_1],
    expectedValue,
    "Environment contains the correct preference value."
  );
  Assert.equal(
    userPrefs[PREF_TEST_2],
    "<user-set>",
    "Report that the pref was user set but the value is not shown."
  );
  Assert.ok(
    !(PREF_TEST_3 in userPrefs),
    "Do not report if preference not user set."
  );
  Assert.equal(
    userPrefs[PREF_TEST_5],
    expectedValue,
    "The pref value in the environment data should still be the same"
  );
});

add_task(async function test_prefWatch_prefReset() {
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
  ]);

  // Set the preference to a non-default value.
  Preferences.set(PREF_TEST, false);

  // Set the Environment preferences to watch.
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "testWatchPrefs_reset",
    deferred.resolve
  );

  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST],
    "<user-set>"
  );

  // Trigger a change in the watched preferences.
  Preferences.reset(PREF_TEST);
  await deferred.promise;

  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST],
    undefined
  );

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_reset");
});

add_task(async function test_prefDefault() {
  const PREF_TEST = "toolkit.telemetry.test.defaultpref1";
  const expectedValue = "some-test-value";

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_DEFAULTPREF_VALUE }],
  ]);

  // Set the preference to a default value.
  Services.prefs.getDefaultBranch(null).setCharPref(PREF_TEST, expectedValue);

  // Set the Environment preferences to watch.
  // We're not watching, but this function does the setup we need.
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST],
    expectedValue
  );
});

add_task(async function test_prefDefaultState() {
  const PREF_TEST = "toolkit.telemetry.test.defaultpref2";
  const expectedValue = "some-test-value";

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_DEFAULTPREF_STATE }],
  ]);

  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

  Assert.equal(
    PREF_TEST in TelemetryEnvironment.currentEnvironment.settings.userPrefs,
    false
  );

  // Set the preference to a default value.
  Services.prefs.getDefaultBranch(null).setCharPref(PREF_TEST, expectedValue);

  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST],
    "<set>"
  );
});

add_task(async function test_prefInvalid() {
  const PREF_TEST_1 = "toolkit.telemetry.test.invalid1";
  const PREF_TEST_2 = "toolkit.telemetry.test.invalid2";

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST_1, { what: TelemetryEnvironment.RECORD_DEFAULTPREF_VALUE }],
    [PREF_TEST_2, { what: TelemetryEnvironment.RECORD_DEFAULTPREF_STATE }],
  ]);

  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_1],
    undefined
  );
  Assert.strictEqual(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[PREF_TEST_2],
    undefined
  );
});

add_task(async function test_addonsWatch_InterestingChange() {
  const ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";
  const ADDON_ID = "tel-restartless-webext@tests.mozilla.org";
  // We only expect a single notification for each install, uninstall, enable, disable.
  const EXPECTED_NOTIFICATIONS = 4;

  let receivedNotifications = 0;

  let registerCheckpointPromise = aExpected => {
    return new Promise(resolve =>
      TelemetryEnvironment.registerChangeListener(
        "testWatchAddons_Changes" + aExpected,
        (reason, data) => {
          Assert.equal(reason, "addons-changed");
          receivedNotifications++;
          resolve();
        }
      )
    );
  };

  let assertCheckpoint = aExpected => {
    Assert.equal(receivedNotifications, aExpected);
    TelemetryEnvironment.unregisterChangeListener(
      "testWatchAddons_Changes" + aExpected
    );
  };

  // Test for receiving one notification after each change.
  let checkpointPromise = registerCheckpointPromise(1);
  await installXPIFromURL(ADDON_INSTALL_URL);
  await checkpointPromise;
  assertCheckpoint(1);
  Assert.ok(
    ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons
  );

  checkpointPromise = registerCheckpointPromise(2);
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.disable();
  await checkpointPromise;
  assertCheckpoint(2);
  Assert.ok(
    !(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons)
  );

  checkpointPromise = registerCheckpointPromise(3);
  let startupPromise = AddonTestUtils.promiseWebExtensionStartup(ADDON_ID);
  await addon.enable();
  await checkpointPromise;
  assertCheckpoint(3);
  Assert.ok(
    ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons
  );
  await startupPromise;

  checkpointPromise = registerCheckpointPromise(4);
  (await AddonManager.getAddonByID(ADDON_ID)).uninstall();
  await checkpointPromise;
  assertCheckpoint(4);
  Assert.ok(
    !(ADDON_ID in TelemetryEnvironment.currentEnvironment.addons.activeAddons)
  );

  Assert.equal(
    receivedNotifications,
    EXPECTED_NOTIFICATIONS,
    "We must only receive the notifications we expect."
  );
});

add_task(async function test_pluginsWatch_Add() {
  if (gIsAndroid) {
    Assert.ok(true, "Skipping: there is no Plugin Manager on Android.");
    return;
  }

  Assert.equal(
    TelemetryEnvironment.currentEnvironment.addons.activePlugins.length,
    1
  );

  let newPlugin = new PluginTag(
    PLUGIN2_NAME,
    PLUGIN2_DESC,
    PLUGIN2_VERSION,
    true
  );
  gInstalledPlugins.push(newPlugin);

  let deferred = PromiseUtils.defer();
  let receivedNotifications = 0;
  let callback = (reason, data) => {
    receivedNotifications++;
    Assert.equal(reason, "addons-changed");
    deferred.resolve();
  };
  TelemetryEnvironment.registerChangeListener("testWatchPlugins_Add", callback);

  Services.obs.notifyObservers(null, PLUGIN_UPDATED_TOPIC);
  await deferred.promise;

  Assert.equal(
    TelemetryEnvironment.currentEnvironment.addons.activePlugins.length,
    2
  );

  TelemetryEnvironment.unregisterChangeListener("testWatchPlugins_Add");

  Assert.equal(
    receivedNotifications,
    1,
    "We must only receive one notification."
  );
});

add_task(async function test_pluginsWatch_Remove() {
  if (gIsAndroid) {
    Assert.ok(true, "Skipping: there is no Plugin Manager on Android.");
    return;
  }

  // Find the test plugin.
  let plugin = gInstalledPlugins.find(p => p.name == PLUGIN2_NAME);
  Assert.ok(plugin, "The test plugin must exist.");

  // Remove it from the PluginHost.
  gInstalledPlugins = gInstalledPlugins.filter(p => p != plugin);

  let deferred = PromiseUtils.defer();
  let receivedNotifications = 0;
  let callback = () => {
    receivedNotifications++;
    deferred.resolve();
  };
  TelemetryEnvironment.registerChangeListener(
    "testWatchPlugins_Remove",
    callback
  );

  Services.obs.notifyObservers(null, PLUGIN_UPDATED_TOPIC);
  await deferred.promise;

  TelemetryEnvironment.unregisterChangeListener("testWatchPlugins_Remove");

  Assert.equal(
    receivedNotifications,
    1,
    "We must only receive one notification."
  );
});

add_task(async function test_addonsWatch_NotInterestingChange() {
  // We are not interested to dictionary addons changes.
  const DICTIONARY_ADDON_INSTALL_URL = gDataRoot + "dictionary.xpi";
  const INTERESTING_ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";

  let receivedNotification = false;
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testNotInteresting", () => {
    Assert.ok(
      !receivedNotification,
      "Should not receive multiple notifications"
    );
    receivedNotification = true;
    deferred.resolve();
  });

  let dictionaryAddon = await installXPIFromURL(DICTIONARY_ADDON_INSTALL_URL);
  let interestingAddon = await installXPIFromURL(INTERESTING_ADDON_INSTALL_URL);

  await deferred.promise;
  Assert.ok(
    !(
      "telemetry-dictionary@tests.mozilla.org" in
      TelemetryEnvironment.currentEnvironment.addons.activeAddons
    ),
    "Dictionaries should not appear in active addons."
  );

  TelemetryEnvironment.unregisterChangeListener("testNotInteresting");

  dictionaryAddon.uninstall();
  await interestingAddon.startupPromise;
  interestingAddon.uninstall();
});

add_task(async function test_addonsAndPlugins() {
  const ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";
  const ADDON_ID = "tel-restartless-webext@tests.mozilla.org";
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
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    isSystem: false,
    isWebExtension: true,
    multiprocessCompatible: true,
  };
  const SYSTEM_ADDON_ID = "tel-system-xpi@tests.mozilla.org";
  const EXPECTED_SYSTEM_ADDON_DATA = {
    blocklisted: false,
    description: "A system addon which is shipped with Firefox.",
    name: "XPI Telemetry System Add-on Test",
    userDisabled: false,
    appDisabled: false,
    version: "1.0",
    scope: 1,
    type: "extension",
    foreignInstall: false,
    hasBinaryComponents: false,
    installDay: truncateToDays(SYSTEM_ADDON_INSTALL_DATE),
    updateDay: truncateToDays(SYSTEM_ADDON_INSTALL_DATE),
    signedState: undefined,
    isSystem: true,
    isWebExtension: true,
    multiprocessCompatible: true,
  };

  const WEBEXTENSION_ADDON_ID = "tel-webextension-xpi@tests.mozilla.org";
  const WEBEXTENSION_ADDON_INSTALL_DATE = truncateToDays(Date.now());
  const EXPECTED_WEBEXTENSION_ADDON_DATA = {
    blocklisted: false,
    description: "A webextension addon.",
    name: "XPI Telemetry WebExtension Add-on Test",
    userDisabled: false,
    appDisabled: false,
    version: "1.0",
    scope: 1,
    type: "extension",
    foreignInstall: false,
    hasBinaryComponents: false,
    installDay: WEBEXTENSION_ADDON_INSTALL_DATE,
    updateDay: WEBEXTENSION_ADDON_INSTALL_DATE,
    signedState: AddonManager.SIGNEDSTATE_PRIVILEGED,
    isSystem: false,
    isWebExtension: true,
    multiprocessCompatible: true,
  };

  const EXPECTED_PLUGIN_DATA = {
    name: FLASH_PLUGIN_NAME,
    version: FLASH_PLUGIN_VERSION,
    description: FLASH_PLUGIN_DESC,
    blocklisted: false,
    disabled: false,
    clicktoplay: true,
  };

  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_WebExtension",
    (reason, data) => {
      Assert.equal(reason, "addons-changed");
      deferred.resolve();
    }
  );

  // Install an add-on so we have some data.
  let addon = await installXPIFromURL(ADDON_INSTALL_URL);

  // Install a webextension as well.
  ExtensionTestUtils.init(this);

  let webextension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name: "XPI Telemetry WebExtension Add-on Test",
      description: "A webextension addon.",
      version: "1.0",
      applications: {
        gecko: {
          id: WEBEXTENSION_ADDON_ID,
        },
      },
    },
  });

  await webextension.startup();
  await deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("test_WebExtension");

  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // Check addon data.
  Assert.ok(
    ADDON_ID in data.addons.activeAddons,
    "We must have one active addon."
  );
  let targetAddon = data.addons.activeAddons[ADDON_ID];
  for (let f in EXPECTED_ADDON_DATA) {
    Assert.equal(
      targetAddon[f],
      EXPECTED_ADDON_DATA[f],
      f + " must have the correct value."
    );
  }

  // Check system add-on data.
  Assert.ok(
    SYSTEM_ADDON_ID in data.addons.activeAddons,
    "We must have one active system addon."
  );
  let targetSystemAddon = data.addons.activeAddons[SYSTEM_ADDON_ID];
  for (let f in EXPECTED_SYSTEM_ADDON_DATA) {
    Assert.equal(
      targetSystemAddon[f],
      EXPECTED_SYSTEM_ADDON_DATA[f],
      f + " must have the correct value."
    );
  }

  // Check webextension add-on data.
  Assert.ok(
    WEBEXTENSION_ADDON_ID in data.addons.activeAddons,
    "We must have one active webextension addon."
  );
  let targetWebExtensionAddon = data.addons.activeAddons[WEBEXTENSION_ADDON_ID];
  for (let f in EXPECTED_WEBEXTENSION_ADDON_DATA) {
    Assert.equal(
      targetWebExtensionAddon[f],
      EXPECTED_WEBEXTENSION_ADDON_DATA[f],
      f + " must have the correct value."
    );
  }

  await webextension.unload();

  // Check plugin data.
  Assert.equal(
    data.addons.activePlugins.length,
    1,
    "We must have only one active plugin."
  );
  let targetPlugin = data.addons.activePlugins[0];
  for (let f in EXPECTED_PLUGIN_DATA) {
    Assert.equal(
      targetPlugin[f],
      EXPECTED_PLUGIN_DATA[f],
      f + " must have the correct value."
    );
  }

  // Check plugin mime types.
  Assert.ok(targetPlugin.mimeTypes.find(m => m == PLUGIN_MIME_TYPE1));
  Assert.ok(targetPlugin.mimeTypes.find(m => m == PLUGIN_MIME_TYPE2));
  Assert.ok(!targetPlugin.mimeTypes.find(m => m == "Not There."));

  // Uninstall the addon.
  await addon.startupPromise;
  await addon.uninstall();
});

add_task(async function test_signedAddon() {
  AddonTestUtils.useRealCertChecks = true;

  const ADDON_INSTALL_URL = gDataRoot + "signed-webext.xpi";
  const ADDON_ID = "tel-signed-webext@tests.mozilla.org";
  const ADDON_INSTALL_DATE = truncateToDays(Date.now());
  const EXPECTED_ADDON_DATA = {
    blocklisted: false,
    description: "A signed webextension",
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
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
  };

  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_signedAddon",
    deferred.resolve
  );

  // Install the addon.
  let addon = await installXPIFromURL(ADDON_INSTALL_URL);

  await deferred.promise;
  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("test_signedAddon");

  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // Check addon data.
  Assert.ok(
    ADDON_ID in data.addons.activeAddons,
    "Add-on should be in the environment."
  );
  let targetAddon = data.addons.activeAddons[ADDON_ID];
  for (let f in EXPECTED_ADDON_DATA) {
    Assert.equal(
      targetAddon[f],
      EXPECTED_ADDON_DATA[f],
      f + " must have the correct value."
    );
  }

  AddonTestUtils.useRealCertChecks = false;
  await addon.startupPromise;
  await addon.uninstall();
});

add_task(async function test_addonsFieldsLimit() {
  const ADDON_INSTALL_URL = gDataRoot + "long-fields.xpi";
  const ADDON_ID = "tel-longfields-webext@tests.mozilla.org";

  // Install the addon and wait for the TelemetryEnvironment to pick it up.
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_longFieldsAddon",
    deferred.resolve
  );
  let addon = await installXPIFromURL(ADDON_INSTALL_URL);
  await deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("test_longFieldsAddon");

  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // Check that the addon is available and that the string fields are limited.
  Assert.ok(
    ADDON_ID in data.addons.activeAddons,
    "Add-on should be in the environment."
  );
  let targetAddon = data.addons.activeAddons[ADDON_ID];

  // TelemetryEnvironment limits the length of string fields for activeAddons to 100 chars,
  // to mitigate misbehaving addons.
  Assert.lessOrEqual(
    targetAddon.version.length,
    100,
    "The version string must have been limited"
  );
  Assert.lessOrEqual(
    targetAddon.name.length,
    100,
    "The name string must have been limited"
  );
  Assert.lessOrEqual(
    targetAddon.description.length,
    100,
    "The description string must have been limited"
  );

  await addon.startupPromise;
  await addon.uninstall();
});

add_task(async function test_collectionWithbrokenAddonData() {
  const BROKEN_ADDON_ID = "telemetry-test2.example.com@services.mozilla.org";
  const BROKEN_MANIFEST = {
    id: "telemetry-test2.example.com@services.mozilla.org",
    name: "telemetry broken addon",
    origin: "https://telemetry-test2.example.com",
    version: 1, // This is intentionally not a string.
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
    type: "extension",
  };

  const ADDON_INSTALL_URL = gDataRoot + "restartless.xpi";
  const ADDON_ID = "tel-restartless-webext@tests.mozilla.org";
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
    signedState: AddonManager.SIGNEDSTATE_MISSING,
  };

  let receivedNotifications = 0;

  let registerCheckpointPromise = aExpected => {
    return new Promise(resolve =>
      TelemetryEnvironment.registerChangeListener(
        "testBrokenAddon_collection" + aExpected,
        (reason, data) => {
          Assert.equal(reason, "addons-changed");
          receivedNotifications++;
          resolve();
        }
      )
    );
  };

  let assertCheckpoint = aExpected => {
    Assert.equal(receivedNotifications, aExpected);
    TelemetryEnvironment.unregisterChangeListener(
      "testBrokenAddon_collection" + aExpected
    );
  };

  // Register the broken provider and install the broken addon.
  let checkpointPromise = registerCheckpointPromise(1);
  let brokenAddonProvider = createMockAddonProvider(
    "Broken Extensions Provider"
  );
  AddonManagerPrivate.registerProvider(brokenAddonProvider);
  brokenAddonProvider.addAddon(BROKEN_MANIFEST);
  await checkpointPromise;
  assertCheckpoint(1);

  // Now install an addon which returns the correct information.
  checkpointPromise = registerCheckpointPromise(2);
  let addon = await installXPIFromURL(ADDON_INSTALL_URL);
  await checkpointPromise;
  assertCheckpoint(2);

  // Check that the new environment contains the Social addon installed with the broken
  // manifest and the rest of the data.
  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data, { expectBrokenAddons: true });

  let activeAddons = data.addons.activeAddons;
  Assert.ok(
    BROKEN_ADDON_ID in activeAddons,
    "The addon with the broken manifest must be reported."
  );
  Assert.equal(
    activeAddons[BROKEN_ADDON_ID].version,
    null,
    "null should be reported for invalid data."
  );
  Assert.ok(ADDON_ID in activeAddons, "The valid addon must be reported.");
  Assert.equal(
    activeAddons[ADDON_ID].description,
    EXPECTED_ADDON_DATA.description,
    "The description for the valid addon should be correct."
  );

  // Unregister the broken provider so we don't mess with other tests.
  AddonManagerPrivate.unregisterProvider(brokenAddonProvider);

  // Uninstall the valid addon.
  await addon.startupPromise;
  await addon.uninstall();
});

add_task(async function test_defaultSearchEngine() {
  // Check that no default engine is in the environment before the search service is
  // initialized.
  let searchExtensions = do_get_cwd();
  searchExtensions.append("data");
  searchExtensions.append("search-extensions");
  let resProt = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution(
    "search-extensions",
    Services.io.newURI("file://" + searchExtensions.path)
  );

  let data = await TelemetryEnvironment.testCleanRestart().onInitialized();
  checkEnvironmentData(data);
  Assert.ok(!("defaultSearchEngine" in data.settings));
  Assert.ok(!("defaultSearchEngineData" in data.settings));

  // Load the engines definitions from a xpcshell data: that's needed so that
  // the search provider reports an engine identifier.

  // Initialize the search service.
  await Services.search.init();
  await promiseNextTick();

  // Our default engine from the JAR file has an identifier. Check if it is correctly
  // reported.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, "telemetrySearchIdentifier");
  let expectedSearchEngineData = {
    name: "telemetrySearchIdentifier",
    loadPath:
      "[other]addEngineWithDetails:telemetrySearchIdentifier@search.mozilla.org",
    origin: "default",
    submissionURL:
      "https://ar.wikipedia.org/wiki/%D8%AE%D8%A7%D8%B5:%D8%A8%D8%AD%D8%AB?search=&sourceId=Mozilla-search",
  };
  Assert.deepEqual(
    data.settings.defaultSearchEngineData,
    expectedSearchEngineData
  );

  // Remove all the search engines.
  for (let engine of await Services.search.getEngines()) {
    await Services.search.removeEngine(engine);
  }
  // The search service does not notify "engine-default" when removing a default engine.
  // Manually force the notification.
  // TODO: remove this when bug 1165341 is resolved.
  Services.obs.notifyObservers(
    null,
    "browser-search-engine-modified",
    "engine-default"
  );
  await promiseNextTick();

  // Then check that no default engine is reported if none is available.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, "NONE");
  Assert.deepEqual(data.settings.defaultSearchEngineData, { name: "NONE" });

  // Add a new search engine (this will have no engine identifier).
  const SEARCH_ENGINE_ID = "telemetry_default";
  const SEARCH_ENGINE_URL = "http://www.example.org/?search={searchTerms}";
  await Services.search.addEngineWithDetails(SEARCH_ENGINE_ID, {
    method: "get",
    template: SEARCH_ENGINE_URL,
  });

  // Register a new change listener and then wait for the search engine change to be notified.
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "testWatch_SearchDefault",
    deferred.resolve
  );
  await Services.search.setDefault(
    Services.search.getEngineByName(SEARCH_ENGINE_ID)
  );
  await deferred.promise;

  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  const EXPECTED_SEARCH_ENGINE = "other-" + SEARCH_ENGINE_ID;
  Assert.equal(data.settings.defaultSearchEngine, EXPECTED_SEARCH_ENGINE);

  const EXPECTED_SEARCH_ENGINE_DATA = {
    name: "telemetry_default",
    loadPath: "[other]addEngineWithDetails",
    origin: "verified",
  };
  Assert.deepEqual(
    data.settings.defaultSearchEngineData,
    EXPECTED_SEARCH_ENGINE_DATA
  );
  TelemetryEnvironment.unregisterChangeListener("testWatch_SearchDefault");

  // Cleanly install an engine from an xml file, and check if origin is
  // recorded as "verified".
  let promise = new Promise(resolve => {
    TelemetryEnvironment.registerChangeListener(
      "testWatch_SearchDefault",
      resolve
    );
  });
  let engine = await new Promise((resolve, reject) => {
    Services.obs.addObserver(function obs(obsSubject, obsTopic, obsData) {
      try {
        let searchEngine = obsSubject.QueryInterface(Ci.nsISearchEngine);
        info("Observed " + obsData + " for " + searchEngine.name);
        if (
          obsData != "engine-added" ||
          searchEngine.name != "engine-telemetry"
        ) {
          return;
        }

        Services.obs.removeObserver(obs, "browser-search-engine-modified");
        resolve(searchEngine);
      } catch (ex) {
        reject(ex);
      }
    }, "browser-search-engine-modified");
    Services.search.addEngine(
      "file://" + do_get_cwd().path + "/engine.xml",
      null,
      false
    );
  });
  await Services.search.setDefault(engine);
  await promise;
  TelemetryEnvironment.unregisterChangeListener("testWatch_SearchDefault");
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.deepEqual(data.settings.defaultSearchEngineData, {
    name: "engine-telemetry",
    loadPath: "[other]/engine.xml",
    origin: "verified",
  });

  // Now break this engine's load path hash.
  promise = new Promise(resolve => {
    TelemetryEnvironment.registerChangeListener(
      "testWatch_SearchDefault",
      resolve
    );
  });
  engine.wrappedJSObject.setAttr("loadPathHash", "broken");
  Services.obs.notifyObservers(
    null,
    "browser-search-engine-modified",
    "engine-default"
  );
  await promise;
  TelemetryEnvironment.unregisterChangeListener("testWatch_SearchDefault");
  data = TelemetryEnvironment.currentEnvironment;
  Assert.equal(data.settings.defaultSearchEngineData.origin, "invalid");
  await Services.search.removeEngine(engine);

  // Define and reset the test preference.
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
  ]);
  Preferences.reset(PREF_TEST);

  // Watch the test preference.
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "testSearchEngine_pref",
    deferred.resolve
  );
  // Trigger an environment change.
  Preferences.set(PREF_TEST, 1);
  await deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("testSearchEngine_pref");

  // Check that the search engine information is correctly retained when prefs change.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.equal(data.settings.defaultSearchEngine, EXPECTED_SEARCH_ENGINE);

  // Check that by default we are not sending a cohort identifier...
  Assert.equal(data.settings.searchCohort, undefined);

  // ... but that if a cohort identifier is set, we send it.
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "testSearchEngine_pref",
    deferred.resolve
  );
  Services.prefs.setCharPref("browser.search.cohort", "testcohort");
  Services.obs.notifyObservers(null, "browser-search-service", "init-complete");
  await deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("testSearchEngine_pref");
  data = TelemetryEnvironment.currentEnvironment;
  Assert.equal(data.settings.searchCohort, "testcohort");
  Assert.equal(data.experiments.searchCohort.branch, "testcohort");

  // Check that when changing the cohort identifier...
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "testSearchEngine_pref",
    deferred.resolve
  );
  Services.prefs.setCharPref("browser.search.cohort", "testcohort2");
  Services.obs.notifyObservers(null, "browser-search-service", "init-complete");
  await deferred.promise;
  TelemetryEnvironment.unregisterChangeListener("testSearchEngine_pref");
  data = TelemetryEnvironment.currentEnvironment;
  // ... the setting and experiment are updated.
  Assert.equal(data.settings.searchCohort, "testcohort2");
  Assert.equal(data.experiments.searchCohort.branch, "testcohort2");
});

add_task(
  { skip_if: () => AppConstants.MOZ_APP_NAME == "thunderbird" },
  async function test_delayed_defaultBrowser() {
    // Skip this test on Thunderbird since it is not a browser, so it cannot
    // be the default browser.

    // Make sure we don't have anything already cached for this test.
    await TelemetryEnvironment.testCleanRestart().onInitialized();

    let environmentData = TelemetryEnvironment.currentEnvironment;
    checkEnvironmentData(environmentData);
    Assert.equal(
      environmentData.settings.isDefaultBrowser,
      null,
      "isDefaultBrowser must be null before the session is restored."
    );

    Services.obs.notifyObservers(null, "sessionstore-windows-restored");

    environmentData = TelemetryEnvironment.currentEnvironment;
    checkEnvironmentData(environmentData);
    Assert.ok(
      "isDefaultBrowser" in environmentData.settings,
      "isDefaultBrowser must be available after the session is restored."
    );
    Assert.equal(
      typeof environmentData.settings.isDefaultBrowser,
      "boolean",
      "isDefaultBrowser must be of the right type."
    );

    // Make sure pref-flipping doesn't overwrite the browser default state.
    const PREF_TEST = "toolkit.telemetry.test.pref1";
    const PREFS_TO_WATCH = new Map([
      [PREF_TEST, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
    ]);
    Preferences.reset(PREF_TEST);

    // Watch the test preference.
    await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
    let deferred = PromiseUtils.defer();
    TelemetryEnvironment.registerChangeListener(
      "testDefaultBrowser_pref",
      deferred.resolve
    );
    // Trigger an environment change.
    Preferences.set(PREF_TEST, 1);
    await deferred.promise;
    TelemetryEnvironment.unregisterChangeListener("testDefaultBrowser_pref");

    // Check that the data is still available.
    environmentData = TelemetryEnvironment.currentEnvironment;
    checkEnvironmentData(environmentData);
    Assert.ok(
      "isDefaultBrowser" in environmentData.settings,
      "isDefaultBrowser must still be available after a pref is flipped."
    );
  }
);

add_task(async function test_osstrings() {
  // First test that numbers in sysinfo properties are converted to string fields
  // in system.os.
  SysInfo.overrides = {
    version: 1,
    name: 2,
    kernel_version: 3,
  };

  await TelemetryEnvironment.testCleanRestart().onInitialized();
  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  Assert.equal(data.system.os.version, "1");
  Assert.equal(data.system.os.name, "2");
  if (AppConstants.platform == "android") {
    Assert.equal(data.system.os.kernelVersion, "3");
  }

  // Check that null values are also handled.
  SysInfo.overrides = {
    version: null,
    name: null,
    kernel_version: null,
  };

  await TelemetryEnvironment.testCleanRestart().onInitialized();
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  Assert.equal(data.system.os.version, null);
  Assert.equal(data.system.os.name, null);
  if (AppConstants.platform == "android") {
    Assert.equal(data.system.os.kernelVersion, null);
  }

  // Clean up.
  SysInfo.overrides = {};
  await TelemetryEnvironment.testCleanRestart().onInitialized();
});

add_task(async function test_experimentsAPI() {
  const EXPERIMENT1 = "experiment-1";
  const EXPERIMENT1_BRANCH = "nice-branch";
  const EXPERIMENT2 = "experiment-2";
  const EXPERIMENT2_BRANCH = "other-branch";

  let checkExperiment = (environmentData, id, branch, type = null) => {
    Assert.ok(
      "experiments" in environmentData,
      "The current environment must report the experiment annotations."
    );
    Assert.ok(
      id in environmentData.experiments,
      "The experiments section must contain the expected experiment id."
    );
    Assert.equal(
      environmentData.experiments[id].branch,
      branch,
      "The experiment branch must be correct."
    );
  };

  // Clean the environment and check that it's reporting the correct info.
  await TelemetryEnvironment.testCleanRestart().onInitialized();
  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // We don't expect the experiments section to be there if no annotation
  // happened.
  Assert.ok(
    !("experiments" in data),
    "No experiments section must be reported if nothing was annotated."
  );

  // Add a change listener and add an experiment annotation.
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_experimentsAPI",
    (reason, env) => {
      deferred.resolve(env);
    }
  );
  TelemetryEnvironment.setExperimentActive(EXPERIMENT1, EXPERIMENT1_BRANCH);
  let eventEnvironmentData = await deferred.promise;

  // Check that the old environment does not contain the experiments.
  checkEnvironmentData(eventEnvironmentData);
  Assert.ok(
    !("experiments" in eventEnvironmentData),
    "No experiments section must be reported in the old environment."
  );

  // Check that the current environment contains the right experiment.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  checkExperiment(data, EXPERIMENT1, EXPERIMENT1_BRANCH);

  TelemetryEnvironment.unregisterChangeListener("test_experimentsAPI");

  // Add a second annotation and check that both experiments are there.
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_experimentsAPI2",
    (reason, env) => {
      deferred.resolve(env);
    }
  );
  TelemetryEnvironment.setExperimentActive(EXPERIMENT2, EXPERIMENT2_BRANCH);
  eventEnvironmentData = await deferred.promise;

  // Check that the current environment contains both the experiment.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  checkExperiment(data, EXPERIMENT1, EXPERIMENT1_BRANCH);
  checkExperiment(data, EXPERIMENT2, EXPERIMENT2_BRANCH);

  // The previous environment should only contain the first experiment.
  checkExperiment(eventEnvironmentData, EXPERIMENT1, EXPERIMENT1_BRANCH);
  Assert.ok(
    !(EXPERIMENT2 in eventEnvironmentData),
    "The old environment must not contain the new experiment annotation."
  );

  TelemetryEnvironment.unregisterChangeListener("test_experimentsAPI2");

  // Check that removing an unknown experiment annotation does not trigger
  // a notification.
  TelemetryEnvironment.registerChangeListener("test_experimentsAPI3", () => {
    Assert.ok(
      false,
      "Removing an unknown experiment annotation must not trigger a change."
    );
  });
  TelemetryEnvironment.setExperimentInactive("unknown-experiment-id");
  // Also make sure that passing non-string parameters arguments doesn't throw nor
  // trigger a notification.
  TelemetryEnvironment.setExperimentActive({}, "some-branch");
  TelemetryEnvironment.setExperimentActive("some-id", {});
  TelemetryEnvironment.unregisterChangeListener("test_experimentsAPI3");

  // Check that removing a known experiment leaves the other in place and triggers
  // a change.
  deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener(
    "test_experimentsAPI4",
    (reason, env) => {
      deferred.resolve(env);
    }
  );
  TelemetryEnvironment.setExperimentInactive(EXPERIMENT1);
  eventEnvironmentData = await deferred.promise;

  // Check that the current environment contains just the second experiment.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.ok(
    !(EXPERIMENT1 in data),
    "The current environment must not contain the removed experiment annotation."
  );
  checkExperiment(data, EXPERIMENT2, EXPERIMENT2_BRANCH);

  // The previous environment should contain both annotations.
  checkExperiment(eventEnvironmentData, EXPERIMENT1, EXPERIMENT1_BRANCH);
  checkExperiment(eventEnvironmentData, EXPERIMENT2, EXPERIMENT2_BRANCH);

  // Set an experiment with a type and check that it correctly shows up.
  TelemetryEnvironment.setExperimentActive(
    "typed-experiment",
    "random-branch",
    { type: "ab-test" }
  );
  data = TelemetryEnvironment.currentEnvironment;
  checkExperiment(data, "typed-experiment", "random-branch", "ab-test");
});

add_task(async function test_experimentsAPI_limits() {
  const EXPERIMENT =
    "experiment-2-experiment-2-experiment-2-experiment-2-experiment-2" +
    "-experiment-2-experiment-2-experiment-2-experiment-2";
  const EXPERIMENT_BRANCH =
    "other-branch-other-branch-other-branch-other-branch-other" +
    "-branch-other-branch-other-branch-other-branch-other-branch";
  const EXPERIMENT_TRUNCATED = EXPERIMENT.substring(0, 100);
  const EXPERIMENT_BRANCH_TRUNCATED = EXPERIMENT_BRANCH.substring(0, 100);

  // Clean the environment and check that it's reporting the correct info.
  await TelemetryEnvironment.testCleanRestart().onInitialized();
  let data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);

  // We don't expect the experiments section to be there if no annotation
  // happened.
  Assert.ok(
    !("experiments" in data),
    "No experiments section must be reported if nothing was annotated."
  );

  // Add a change listener and wait for the annotation to happen.
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("test_experimentsAPI", () =>
    deferred.resolve()
  );
  TelemetryEnvironment.setExperimentActive(EXPERIMENT, EXPERIMENT_BRANCH);
  await deferred.promise;

  // Check that the current environment contains the truncated values
  // for the experiment data.
  data = TelemetryEnvironment.currentEnvironment;
  checkEnvironmentData(data);
  Assert.ok(
    "experiments" in data,
    "The environment must contain an experiments section."
  );
  Assert.ok(
    EXPERIMENT_TRUNCATED in data.experiments,
    "The experiments must be reporting the truncated id."
  );
  Assert.ok(
    !(EXPERIMENT in data.experiments),
    "The experiments must not be reporting the full id."
  );
  Assert.equal(
    EXPERIMENT_BRANCH_TRUNCATED,
    data.experiments[EXPERIMENT_TRUNCATED].branch,
    "The experiments must be reporting the truncated branch."
  );

  TelemetryEnvironment.unregisterChangeListener("test_experimentsAPI");

  // Check that an overly long type is truncated.
  const longType = "a0123456678901234567890123456789";
  TelemetryEnvironment.setExperimentActive("exp", "some-branch", {
    type: longType,
  });
  data = TelemetryEnvironment.currentEnvironment;
  Assert.equal(data.experiments.exp.type, longType.substring(0, 20));
});

if (gIsWindows) {
  add_task(async function test_environmentHDDInfo() {
    await TelemetryEnvironment.testCleanRestart().onInitialized();
    let data = TelemetryEnvironment.currentEnvironment;
    let empty = { model: null, revision: null, type: null };
    Assert.deepEqual(
      data.system.hdd,
      { binary: empty, profile: empty, system: empty },
      "Should have no data yet."
    );
    await TelemetryEnvironment.delayedInit();
    data = TelemetryEnvironment.currentEnvironment;
    for (let k of EXPECTED_HDD_FIELDS) {
      checkString(data.system.hdd[k].model);
      checkString(data.system.hdd[k].revision);
      checkString(data.system.hdd[k].type);
    }
  });
}

add_task(async function test_environmentShutdown() {
  // Define and reset the test preference.
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_PREF_STATE }],
  ]);
  Preferences.reset(PREF_TEST);

  // Set up the preferences and listener, then the trigger shutdown
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  TelemetryEnvironment.registerChangeListener(
    "test_environmentShutdownChange",
    () => {
      // Register a new change listener that asserts if change is propogated
      Assert.ok(false, "No change should be propagated after shutdown.");
    }
  );
  TelemetryEnvironment.shutdown();

  // Flipping  the test preference after shutdown should not trigger the listener
  Preferences.set(PREF_TEST, 1);

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener(
    "test_environmentShutdownChange"
  );
});
