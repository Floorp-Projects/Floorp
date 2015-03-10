/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryEnvironment",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");
#ifndef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
#endif
XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
                                  "resource://gre/modules/ProfileAge.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";

const PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
const PREF_DISTRIBUTION_ID = "distribution.id";
const PREF_DISTRIBUTION_VERSION = "distribution.version";
const PREF_DISTRIBUTOR = "app.distributor";
const PREF_DISTRIBUTOR_CHANNEL = "app.distributor.channel";
const PREF_E10S_ENABLED = "browser.tabs.remote.autostart";
const PREF_HOTFIX_LASTVERSION = "extensions.hotfix.lastVersion";
const PREF_APP_PARTNER_BRANCH = "app.partner.";
const PREF_PARTNER_ID = "mozilla.partner.id";
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
const PREF_UPDATE_ENABLED = "app.update.enabled";
const PREF_UPDATE_AUTODOWNLOAD = "app.update.auto";

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

const EXPERIMENTS_CHANGED_TOPIC = "experiments-changed";

/**
 * Turn a millisecond timestamp into a day timestamp.
 *
 * @param aMsec a number of milliseconds since epoch.
 * @return the number of whole days denoted by the input.
 */
function truncateToDays(aMsec) {
  return Math.floor(aMsec / MILLISECONDS_PER_DAY);
}

/**
 * Get the current browser.
 * @return a string with the locale or null on failure.
 */
function getBrowserLocale() {
  try {
    return Cc["@mozilla.org/chrome/chrome-registry;1"].
             getService(Ci.nsIXULChromeRegistry).
             getSelectedLocale('global');
  } catch (e) {
    return null;
  }
}

/**
 * Get the current OS locale.
 * @return a string with the OS locale or null on failure.
 */
function getSystemLocale() {
  try {
    return Services.locale.getLocaleComponentForUserAgent();
  } catch (e) {
    return null;
  }
}

/**
 * Asynchronously get a list of addons of the specified type from the AddonManager.
 * @param aTypes An array containing the types of addons to request.
 * @return Promise<Array> resolved when AddonManager has finished, returning an
 *         array of addons.
 */
function promiseGetAddonsByTypes(aTypes) {
  return new Promise((resolve) =>
                     AddonManager.getAddonsByTypes(aTypes, (addons) => resolve(addons)));
}

/**
 * Safely get a sysinfo property and return its value. If the property is not
 * available, return aDefault.
 *
 * @param aPropertyName the property name to get.
 * @param aDefault the value to return if aPropertyName is not available.
 * @return The property value, if available, or aDefault.
 */
function getSysinfoProperty(aPropertyName, aDefault) {
  try {
    // |getProperty| may throw if |aPropertyName| does not exist.
    return Services.sysinfo.getProperty(aPropertyName);
  } catch (e) {}

  return aDefault;
}

/**
 * Safely get a gfxInfo field and return its value. If the field is not available, return
 * aDefault.
 *
 * @param aPropertyName the property name to get.
 * @param aDefault the value to return if aPropertyName is not available.
 * @return The property value, if available, or aDefault.
 */
function getGfxField(aPropertyName, aDefault) {
  let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

  try {
    // Accessing the field may throw if |aPropertyName| does not exist.
    let gfxProp = gfxInfo[aPropertyName];
    if (gfxProp !== "") {
      return gfxProp;
    }
  } catch (e) {}

  return aDefault;
}

/**
 * Get the information about a graphic adapter.
 *
 * @param aSuffix A suffix to add to the properties names.
 * @return An object containing the adapter properties.
 */
function getGfxAdapter(aSuffix = "") {
  let memoryMB = getGfxField("adapterRAM" + aSuffix, null);
  if (memoryMB) {
    memoryMB = parseInt(memoryMB, 10);
  }

  return {
    description: getGfxField("adapterDescription" + aSuffix, null),
    vendorID: getGfxField("adapterVendorID" + aSuffix, null),
    deviceID: getGfxField("adapterDeviceID" + aSuffix, null),
    subsysID: getGfxField("adapterSubsysID" + aSuffix, null),
    RAM: memoryMB,
    driver: getGfxField("adapterDriver" + aSuffix, null),
    driverVersion: getGfxField("adapterDriverVersion" + aSuffix, null),
    driverDate: getGfxField("adapterDriverDate" + aSuffix, null),
  };
}

#ifdef XP_WIN
/**
 * Gets the service pack information on Windows platforms. This was copied from
 * nsUpdateService.js.
 *
 * @return An object containing the service pack major and minor versions.
 */
function getServicePack() {
  const BYTE = ctypes.uint8_t;
  const WORD = ctypes.uint16_t;
  const DWORD = ctypes.uint32_t;
  const WCHAR = ctypes.char16_t;
  const BOOL = ctypes.int;

  // This structure is described at:
  // http://msdn.microsoft.com/en-us/library/ms724833%28v=vs.85%29.aspx
  const SZCSDVERSIONLENGTH = 128;
  const OSVERSIONINFOEXW = new ctypes.StructType('OSVERSIONINFOEXW',
      [
      {dwOSVersionInfoSize: DWORD},
      {dwMajorVersion: DWORD},
      {dwMinorVersion: DWORD},
      {dwBuildNumber: DWORD},
      {dwPlatformId: DWORD},
      {szCSDVersion: ctypes.ArrayType(WCHAR, SZCSDVERSIONLENGTH)},
      {wServicePackMajor: WORD},
      {wServicePackMinor: WORD},
      {wSuiteMask: WORD},
      {wProductType: BYTE},
      {wReserved: BYTE}
      ]);

  let kernel32 = ctypes.open("kernel32");
  try {
    let GetVersionEx = kernel32.declare("GetVersionExW",
                                        ctypes.default_abi,
                                        BOOL,
                                        OSVERSIONINFOEXW.ptr);
    let winVer = OSVERSIONINFOEXW();
    winVer.dwOSVersionInfoSize = OSVERSIONINFOEXW.size;

    if(0 === GetVersionEx(winVer.address())) {
      throw("Failure in GetVersionEx (returned 0)");
    }

    return {
      major: winVer.wServicePackMajor,
      minor: winVer.wServicePackMinor,
    };
  } catch (e) {
    return {
      major: null,
      minor: null,
    };
  } finally {
    kernel32.close();
  }
}
#endif

this.TelemetryEnvironment = {
  _shutdown: true,

  // A map of (sync) listeners that will be called on environment changes.
  _changeListeners: new Map(),
  // Async task for collecting the environment data.
  _collectTask: null,

  // Policy to use when saving preferences. Exported for using them in tests.
  RECORD_PREF_STATE: 1, // Don't record the preference value
  RECORD_PREF_VALUE: 2, // We only record user-set prefs.
  RECORD_PREF_NOTIFY_ONLY: 3, // Record nothing, just notify of changes.

  // A map of watched preferences which trigger an Environment change when modified.
  // Every entry contains a recording policy (RECORD_PREF_*).
  _watchedPrefs: null,

  // The Addons change listener, init by |_startWatchingAddons| .
  _addonsListener: null,

  // AddonManager may shut down before us, in which case we cache the addons here.
  // It is always null if the AM didn't shut down before us.
  // If cached, it is an object containing the addon information, suitable for use
  // in the environment data.
  _cachedAddons: null,

  /**
   * Initialize TelemetryEnvironment.
   */
  init: function () {
    if (!this._shutdown) {
      this._log.error("init - Already initialized");
      return;
    }

    this._configureLog();
    this._log.trace("init");
    this._shutdown = false;
    this._startWatchingPrefs();
    this._startWatchingAddons();

    AddonManager.shutdown.addBlocker("TelemetryEnvironment: caching addons",
                                      () => this._blockAddonManagerShutdown(),
                                      () => this._getState());
  },

  /**
   * Shutdown TelemetryEnvironment.
   * @return Promise<> that is resolved when shutdown is finished.
   */
  shutdown: Task.async(function* () {
    if (this._shutdown) {
      if (this._log) {
        this._log.error("shutdown - Already shut down");
      } else {
        Cu.reportError("TelemetryEnvironment.shutdown - Already shut down");
      }
      return;
    }

    this._log.trace("shutdown");
    this._shutdown = true;
    this._stopWatchingPrefs();
    this._stopWatchingAddons();
    this._changeListeners.clear();
    yield this._collectTask;

    this._cachedAddons = null;
  }),

  _configureLog: function () {
    if (this._log) {
      return;
    }
    this._log = Log.repository.getLoggerWithMessagePrefix(
                                 LOGGER_NAME, "TelemetryEnvironment::");
  },

  /**
   * Register a listener for environment changes.
   * It's fine to call this on an unitialized TelemetryEnvironment.
   * @param name The name of the listener - good for debugging purposes.
   * @param listener A JS callback function.
   */
  registerChangeListener: function (name, listener) {
    this._configureLog();
    this._log.trace("registerChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown")
      return;
    }
    this._changeListeners.set(name, listener);
  },

  /**
   * Unregister from listening to environment changes.
   * It's fine to call this on an unitialized TelemetryEnvironment.
   * @param name The name of the listener to remove.
   */
  unregisterChangeListener: function (name) {
    this._configureLog();
    this._log.trace("unregisterChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown")
      return;
    }
    this._changeListeners.delete(name);
  },

  /**
   * Only used in tests, set the preferences to watch.
   * @param aPreferences A map of preferences names and their recording policy.
   */
  _watchPreferences: function (aPreferences) {
    if (this._watchedPrefs) {
      this._stopWatchingPrefs();
    }

    this._watchedPrefs = aPreferences;
    this._startWatchingPrefs();
  },

  /**
   * Get an object containing the values for the watched preferences. Depending on the
   * policy, the value for a preference or whether it was changed by user is reported.
   *
   * @return An object containing the preferences values.
   */
  _getPrefData: function () {
    if (!this._watchedPrefs) {
      return {};
    }

    let prefData = {};
    for (let pref in this._watchedPrefs) {
      // Only record preferences if they are non-default and policy allows recording.
      if (!Preferences.isSet(pref) ||
          this._watchedPrefs[pref] == this.RECORD_PREF_NOTIFY_ONLY) {
        continue;
      }

      // Check the policy for the preference and decide if we need to store its value
      // or whether it changed from the default value.
      let prefValue = undefined;
      if (this._watchedPrefs[pref] == this.RECORD_PREF_STATE) {
        prefValue = null;
      } else {
        prefValue = Preferences.get(pref, null);
      }
      prefData[pref] = prefValue;
    }
    return prefData;
  },

  /**
   * Start watching the preferences.
   */
  _startWatchingPrefs: function () {
    this._log.trace("_startWatchingPrefs - " + this._watchedPrefs);

    if (!this._watchedPrefs) {
      return;
    }

    for (let pref in this._watchedPrefs) {
      Preferences.observe(pref, this._onPrefChanged, this);
    }
  },

  /**
   * Do not receive any more change notifications for the preferences.
   */
  _stopWatchingPrefs: function () {
    this._log.trace("_stopWatchingPrefs");

    if (!this._watchedPrefs) {
      return;
    }

    for (let pref in this._watchedPrefs) {
      Preferences.ignore(pref, this._onPrefChanged, this);
    }

    this._watchedPrefs = null;
  },

  _onPrefChanged: function () {
    this._log.trace("_onPrefChanged");
    this._onEnvironmentChange("pref-changed");
  },

  /**
   * Get the build data in object form.
   * @return Object containing the build data.
   */
  _getBuild: function () {
    let buildData = {
      applicationId: Services.appinfo.ID,
      applicationName: Services.appinfo.name,
      architecture: Services.sysinfo.get("arch"),
      buildId: Services.appinfo.appBuildID,
      version: Services.appinfo.version,
      vendor: Services.appinfo.vendor,
      platformVersion: Services.appinfo.platformVersion,
      xpcomAbi: Services.appinfo.XPCOMABI,
      hotfixVersion: Preferences.get(PREF_HOTFIX_LASTVERSION, null),
    };

    // Add |architecturesInBinary| only for Mac Universal builds.
    if ("@mozilla.org/xpcom/mac-utils;1" in Cc) {
      let macUtils = Cc["@mozilla.org/xpcom/mac-utils;1"].getService(Ci.nsIMacUtils);
      if (macUtils && macUtils.isUniversalBinary) {
        buildData.architecturesInBinary = macUtils.architecturesInBinary;
      }
    }

    return buildData;
  },

  /**
   * Determine if Firefox is the default browser.
   * @returns null on error, true if we are the default browser, or false otherwise.
   */
  _isDefaultBrowser: function () {
    if (!("@mozilla.org/browser/shell-service;1" in Cc)) {
      this._log.error("_isDefaultBrowser - Could not obtain shell service");
      return null;
    }

    let shellService;
    try {
      shellService = Cc["@mozilla.org/browser/shell-service;1"]
                       .getService(Ci.nsIShellService);
    } catch (ex) {
      this._log.error("_isDefaultBrowser - Could not obtain shell service", ex);
      return null;
    }

    if (shellService) {
      try {
        // This uses the same set of flags used by the pref pane.
        return shellService.isDefaultBrowser(false, true) ? true : false;
      } catch (ex) {
        this._log.error("_isDefaultBrowser - Could not determine if default browser", ex);
        return null;
      }
    }

    return null;
  },

  /**
   * Get the settings data in object form.
   * @return Object containing the settings.
   */
  _getSettings: function () {
    let updateChannel = null;
    try {
      updateChannel = UpdateChannel.get();
    } catch (e) {}

    return {
      blocklistEnabled: Preferences.get(PREF_BLOCKLIST_ENABLED, true),
#ifndef MOZ_WIDGET_ANDROID
      isDefaultBrowser: this._isDefaultBrowser(),
#endif
      e10sEnabled: Preferences.get(PREF_E10S_ENABLED, false),
      telemetryEnabled: Preferences.get(PREF_TELEMETRY_ENABLED, false),
      locale: getBrowserLocale(),
      update: {
        channel: updateChannel,
        enabled: Preferences.get(PREF_UPDATE_ENABLED, true),
        autoDownload: Preferences.get(PREF_UPDATE_AUTODOWNLOAD, true),
      },
      userPrefs: this._getPrefData(),
    };
  },

  /**
   * Get the profile data in object form.
   * @return Object containing the profile data.
   */
  _getProfile: Task.async(function* () {
    let profileAccessor = new ProfileAge(null, this._log);

    let creationDate = yield profileAccessor.created;
    let resetDate = yield profileAccessor.reset;

    let profileData = {
      creationDate: truncateToDays(creationDate),
    };

    if (resetDate) {
      profileData.resetDate = truncateToDays(resetDate);
    }
    return profileData;
  }),

  /**
   * Get the partner data in object form.
   * @return Object containing the partner data.
   */
  _getPartner: function () {
    let partnerData = {
      distributionId: Preferences.get(PREF_DISTRIBUTION_ID, null),
      distributionVersion: Preferences.get(PREF_DISTRIBUTION_VERSION, null),
      partnerId: Preferences.get(PREF_PARTNER_ID, null),
      distributor: Preferences.get(PREF_DISTRIBUTOR, null),
      distributorChannel: Preferences.get(PREF_DISTRIBUTOR_CHANNEL, null),
    };

    // Get the PREF_APP_PARTNER_BRANCH branch and append its children to partner data.
    let partnerBranch = Services.prefs.getBranch(PREF_APP_PARTNER_BRANCH);
    partnerData.partnerNames = partnerBranch.getChildList("")

    return partnerData;
  },

  /**
   * Get the CPU information.
   * @return Object containing the CPU information data.
   */
  _getCpuData: function () {
    let cpuData = {
      count: getSysinfoProperty("cpucount", null),
      vendor: null, // TODO: bug 1128472
      family: null, // TODO: bug 1128472
      model: null, // TODO: bug 1128472
      stepping: null, // TODO: bug 1128472
    };

    const CPU_EXTENSIONS = ["hasMMX", "hasSSE", "hasSSE2", "hasSSE3", "hasSSSE3",
                            "hasSSE4A", "hasSSE4_1", "hasSSE4_2", "hasEDSP", "hasARMv6",
                            "hasARMv7", "hasNEON"];

    // Enumerate the available CPU extensions.
    let availableExts = [];
    for (let ext of CPU_EXTENSIONS) {
      try {
        Services.sysinfo.getProperty(ext);
        // If it doesn't throw, add it to the list.
        availableExts.push(ext);
      } catch (e) {}
    }

    cpuData.extensions = availableExts;

    return cpuData;
  },

#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
  /**
   * Get the device information, if we are on a portable device.
   * @return Object containing the device information data.
   */
  _getDeviceData: function () {
    return {
      model: getSysinfoProperty("device", null),
      manufacturer: getSysinfoProperty("manufacturer", null),
      hardware: getSysinfoProperty("hardware", null),
      isTablet: getSysinfoProperty("tablet", null),
    };
  },
#endif

  /**
   * Get the OS information.
   * @return Object containing the OS data.
   */
  _getOSData: function () {
#ifdef XP_WIN
    // Try to get service pack information.
    let servicePack = getServicePack();
#endif

    return {
      name: getSysinfoProperty("name", null),
      version: getSysinfoProperty("version", null),
#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
      kernelVersion: getSysinfoProperty("kernel_version", null),
#elif defined(XP_WIN)
      servicePackMajor: servicePack.major,
      servicePackMinor: servicePack.minor,
#endif
      locale: getSystemLocale(),
    };
  },

  /**
   * Get the HDD information.
   * @return Object containing the HDD data.
   */
  _getHDDData: function () {
    return {
      profile: { // hdd where the profile folder is located
        model: getSysinfoProperty("profileHDDModel", null),
        revision: getSysinfoProperty("profileHDDRevision", null),
      },
      binary:  { // hdd where the application binary is located
        model: getSysinfoProperty("binHDDModel", null),
        revision: getSysinfoProperty("binHDDRevision", null),
      },
      system:  { // hdd where the system files are located
        model: getSysinfoProperty("winHDDModel", null),
        revision: getSysinfoProperty("winHDDRevision", null),
      },
    };
  },

  /**
   * Get the GFX information.
   * @return Object containing the GFX data.
   */
  _getGFXData: function () {
    let gfxData = {
      D2DEnabled: getGfxField("D2DEnabled", null),
      DWriteEnabled: getGfxField("DWriteEnabled", null),
      DWriteVersion: getGfxField("DWriteVersion", null),
      adapters: [],
    };

    // GfxInfo does not yet expose a way to iterate through all the adapters.
    gfxData.adapters.push(getGfxAdapter(""));
    gfxData.adapters[0].GPUActive = true;

    // If we have a second adapter add it to the gfxData.adapters section.
    let hasGPU2 = getGfxField("adapterDeviceID2", null) !== null;
    if (!hasGPU2) {
      this._log.trace("_getGFXData - Only one display adapter detected.");
      return gfxData;
    }

    this._log.trace("_getGFXData - Two display adapters detected.");

    gfxData.adapters.push(getGfxAdapter("2"));
    gfxData.adapters[1].GPUActive = getGfxField("isGPU2Active ", null);

    return gfxData;
  },

  /**
   * Get the system data in object form.
   * @return Object containing the system data.
   */
  _getSystem: function () {
    let memoryMB = getSysinfoProperty("memsize", null);
    if (memoryMB) {
      // Send RAM size in megabytes. Rounding because sysinfo doesn't
      // always provide RAM in multiples of 1024.
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }

    return {
      memoryMB: memoryMB,
#ifdef XP_WIN
      isWow64: getSysinfoProperty("isWow64", null),
#endif
      cpu: this._getCpuData(),
#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
      device: this._getDeviceData(),
#endif
      os: this._getOSData(),
      hdd: this._getHDDData(),
      gfx: this._getGFXData(),
    };
  },

  /**
   * Get the addon data in object form.
   * @return Object containing the addon data.
   *
   * This should only be called from the environment collection task
   * or _blockAddonManagerShutdown, otherwise we risk running this
   * during addon manager shutdown.
   */
  _getActiveAddons: Task.async(function* () {

    // Request addons, asynchronously.
    let allAddons = yield promiseGetAddonsByTypes(["extension", "service"]);

    let activeAddons = {};
    for (let addon of allAddons) {
      // Skip addons which are not active.
      if (!addon.isActive) {
        continue;
      }

      // Make sure to have valid dates.
      let installDate = new Date(Math.max(0, addon.installDate));
      let updateDate = new Date(Math.max(0, addon.updateDate));

      activeAddons[addon.id] = {
        blocklisted: (addon.blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED),
        description: addon.description,
        name: addon.name,
        userDisabled: addon.userDisabled,
        appDisabled: addon.appDisabled,
        version: addon.version,
        scope: addon.scope,
        type: addon.type,
        foreignInstall: addon.foreignInstall,
        hasBinaryComponents: addon.hasBinaryComponents,
        installDay: truncateToDays(installDate.getTime()),
        updateDay: truncateToDays(updateDate.getTime()),
      };
    }

    return activeAddons;
  }),

  /**
   * Get the currently active theme data in object form.
   * @return Object containing the active theme data.
   *
   * This should only be called from the environment collection task
   * or _blockAddonManagerShutdown, otherwise we risk running this
   * during addon manager shutdown.
   */
  _getActiveTheme: Task.async(function* () {
    // Request themes, asynchronously.
    let themes = yield promiseGetAddonsByTypes(["theme"]);

    let activeTheme = {};
    // We only store information about the active theme.
    let theme = themes.find(theme => theme.isActive);
    if (theme) {
      // Make sure to have valid dates.
      let installDate = new Date(Math.max(0, theme.installDate));
      let updateDate = new Date(Math.max(0, theme.updateDate));

      activeTheme = {
        id: theme.id,
        blocklisted: (theme.blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED),
        description: theme.description,
        name: theme.name,
        userDisabled: theme.userDisabled,
        appDisabled: theme.appDisabled,
        version: theme.version,
        scope: theme.scope,
        foreignInstall: theme.foreignInstall,
        hasBinaryComponents: theme.hasBinaryComponents,
        installDay: truncateToDays(installDate.getTime()),
        updateDay: truncateToDays(updateDate.getTime()),
      };
    }

    return activeTheme;
  }),

  /**
   * Get the plugins data in object form.
   * @return Object containing the plugins data.
   */
  _getActivePlugins: function () {
    let pluginTags =
      Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost).getPluginTags({});

    let activePlugins = [];
    for (let tag of pluginTags) {
      // Skip plugins which are not active.
      if (tag.disabled) {
        continue;
      }

      // Make sure to have a valid date.
      let updateDate = new Date(Math.max(0, tag.lastModifiedTime));

      activePlugins.push({
        name: tag.name,
        version: tag.version,
        description: tag.description,
        blocklisted: tag.blocklisted,
        disabled: tag.disabled,
        clicktoplay: tag.clicktoplay,
        mimeTypes: tag.getMimeTypes({}),
        updateDay: truncateToDays(updateDate.getTime()),
      });
    }

    return activePlugins;
  },

  /**
   * Get the GMPlugins data in object form.
   * @return Object containing the GMPlugins data.
   *
   * This should only be called from the environment collection task
   * or _blockAddonManagerShutdown, otherwise we risk running this
   * during addon manager shutdown.
   */
  _getActiveGMPlugins: Task.async(function* () {
    // Request plugins, asynchronously.
    let allPlugins = yield promiseGetAddonsByTypes(["plugin"]);

    let activeGMPlugins = {};
    for (let plugin of allPlugins) {
      // Only get GM Plugin info.
      if (!plugin.isGMPlugin) {
        continue;
      }

      activeGMPlugins[plugin.id] = {
        version: plugin.version,
        userDisabled: plugin.userDisabled,
        applyBackgroundUpdates: plugin.applyBackgroundUpdates,
      };
    }

    return activeGMPlugins;
  }),

  /**
   * Get the active experiment data in object form.
   * @return Object containing the active experiment data.
   */
  _getActiveExperiment: function () {
    let experimentInfo = {};
    try {
      let scope = {};
      Cu.import("resource:///modules/experiments/Experiments.jsm", scope);
      let experiments = scope.Experiments.instance()
      let activeExperiment = experiments.getActiveExperimentID();
      if (activeExperiment) {
        experimentInfo.id = activeExperiment;
        experimentInfo.branch = experiments.getActiveExperimentBranch();
      }
    } catch(e) {
      // If this is not Firefox, the import will fail.
      return experimentInfo;
    }

    return experimentInfo;
  },

  /**
   * Get the addon data in object form.
   * @return Object containing the addon data.
   *
   * This should only be called from the environment collection task
   * or _blockAddonManagerShutdown, otherwise we risk running this
   * during addon manager shutdown.
   */
  _getAddons: Task.async(function* () {
    // AddonManager may have shutdown already, in which case we should have cached addon data.
    // It can't shutdown during the collection here because we have a blocker on the AMs
    // shutdown barrier that waits for the collect task.
    let addons = this._cachedAddons || {};
    if (!this._cachedAddons) {
      addons.activeAddons = yield this._getActiveAddons();
      addons.activeTheme = yield this._getActiveTheme();
      addons.activeGMPlugins = yield this._getActiveGMPlugins();
    }

    let personaId = null;
#ifndef MOZ_WIDGET_GONK
    let theme = LightweightThemeManager.currentTheme;
    if (theme) {
      personaId = theme.id;
    }
#endif

    let addonData = {
      activeAddons: addons.activeAddons,
      theme: addons.activeTheme,
      activePlugins: this._getActivePlugins(),
      activeGMPlugins: addons.activeGMPlugins,
      activeExperiment: this._getActiveExperiment(),
      persona: personaId,
    };

    return addonData;
  }),

  /**
   * Start watching the addons for changes.
   */
  _startWatchingAddons: function () {
    // Define a listener to catch addons changes from the AddonManager. This part is
    // tricky, as we only want to detect when the set of active addons changes without
    // getting double notifications.
    //
    // We identified the following cases:
    //
    // * onEnabled:   Gets called when a restartless addon is enabled. Doesn't get called
    //                if the restartless addon is installed and directly enabled.
    // * onDisabled:  Gets called when disabling a restartless addon or can get called when
    //                uninstalling a restartless addon from the UI (see bug 612168).
    // * onInstalled: Gets called for all addon installs.
    // * onUninstalling: Called the moment before addon uninstall happens.

    this._addonsListener = {
      onEnabled: addon => {
        this._log.trace("_addonsListener - onEnabled " + addon.id);
        this._onActiveAddonsChanged(addon)
      },
      onDisabled: addon => {
        this._log.trace("_addonsListener - onDisabled " + addon.id);
        this._onActiveAddonsChanged(addon);
      },
      onInstalled: addon => {
        this._log.trace("_addonsListener - onInstalled " + addon.id +
                        ", isActive: " + addon.isActive);
        if (addon.isActive) {
          this._onActiveAddonsChanged(addon);
        }
      },
      onUninstalling: (addon, requiresRestart) => {
        this._log.trace("_addonsListener - onUninstalling " + addon.id +
                        ", isActive: " + addon.isActive +
                        ", requiresRestart: " + requiresRestart);
        if (!addon.isActive || requiresRestart) {
          return;
        }
        this._onActiveAddonsChanged(addon);
      },
    };

    AddonManager.addAddonListener(this._addonsListener);

    // Watch for experiment changes as well.
    Services.obs.addObserver(this, EXPERIMENTS_CHANGED_TOPIC, false);
  },

  /**
   * Stop watching addons for changes.
   */
  _stopWatchingAddons: function () {
    if (this._addonsListener) {
      AddonManager.removeAddonListener(this._addonsListener);
      Services.obs.removeObserver(this, EXPERIMENTS_CHANGED_TOPIC);
    }
    this._addonsListener = null;
  },

  /**
   * Triggered when an addon changes its state.
   * @param aAddon The addon which triggered the change.
   */
  _onActiveAddonsChanged: function (aAddon) {
    const INTERESTING_ADDONS = [ "extension", "plugin", "service", "theme" ];

    this._log.trace("_onActiveAddonsChanged - id " + aAddon.id + ", type " + aAddon.type);

    if (INTERESTING_ADDONS.find(addon => addon == aAddon.type)) {
      this._onEnvironmentChange("addons-changed");
    }
  },

  /**
   * Handle experiment change notifications.
   */
  observe: function (aSubject, aTopic, aData) {
    this._log.trace("observe - Topic " + aTopic);

    if (aTopic == EXPERIMENTS_CHANGED_TOPIC) {
      this._onEnvironmentChange("experiment-changed");
    }
  },

  /**
   * Get the environment data in object form.
   * @return Promise<Object> Resolved with the data on success, otherwise rejected.
   */
  getEnvironmentData: function() {
    if (this._shutdown) {
      this._log.error("getEnvironmentData - Already shut down");
      return Promise.reject("Already shutdown");
    }

    this._log.trace("getEnvironmentData");
    if (this._collectTask) {
      return this._collectTask;
    }

    this._collectTask = this._doGetEnvironmentData();
    let clear = () => this._collectTask = null;
    this._collectTask.then(clear, clear);
    return this._collectTask;
  },

  _doGetEnvironmentData: Task.async(function* () {
    this._log.trace("getEnvironmentData");

    // Define the data collection function for each section.
    let sections = {
      "build" : () => this._getBuild(),
      "settings": () => this._getSettings(),
#ifndef MOZ_WIDGET_ANDROID
      "profile": () => this._getProfile(),
#endif
      "partner": () => this._getPartner(),
      "system": () => this._getSystem(),
      "addons": () => this._getAddons(),
    };

    let data = {};
    // Recover from exceptions in the collection functions and populate the data
    // object. We want to recover so that, in the worst-case, we only lose some data
    // sections instead of all.
    for (let s in sections) {
      try {
        data[s] = yield sections[s]();
      } catch (e) {
        this._log.error("_doGetEnvironmentData - There was an exception collecting " + s, e);
      }
    }

    return data;
  }),

  _onEnvironmentChange: function (what) {
    this._log.trace("_onEnvironmentChange for " + what);
    if (this._shutdown) {
      this._log.trace("_onEnvironmentChange - Already shut down.");
      return;
    }

    for (let [name, listener] of this._changeListeners) {
      try {
        this._log.debug("_onEnvironmentChange - calling " + name);
        listener();
      } catch (e) {
        this._log.warning("_onEnvironmentChange - listener " + name + " caught error", e);
      }
    }
  },

  /**
   * This blocks the AddonManager shutdown barrier, it caches addons we might need later.
   * It also lets an active collect task finish first as it may still access the AM.
   */
  _blockAddonManagerShutdown: Task.async(function*() {
    this._log.trace("_blockAddonManagerShutdown");

    this._stopWatchingAddons();

    this._cachedAddons = {
      activeAddons: yield this._getActiveAddons(),
      activeTheme: yield this._getActiveTheme(),
      activeGMPlugins: yield this._getActiveGMPlugins(),
    };

    yield this._collectTask;
  }),

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState: function() {
    return {
      shutdown: this._shutdown,
      hasCollectTask: !!this._collectTask,
      hasAddonsListener: !!this._addonsListener,
      hasCachedAddons: !!this._cachedAddons,
    };
  },
};
