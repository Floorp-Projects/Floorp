/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryEnvironment",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const myScope = this;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/ObjectUtils.jsm");

const Utils = TelemetryUtils;

XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");
#ifndef MOZ_WIDGET_GONK
Cu.import("resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
                                  "resource://gre/modules/LightweightThemeManager.jsm");
#endif
XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
                                  "resource://gre/modules/ProfileAge.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

const CHANGE_THROTTLE_INTERVAL_MS = 5 * 60 * 1000;

/**
 * This is a policy object used to override behavior for testing.
 */
let Policy = {
  now: () => new Date(),
};

var gGlobalEnvironment;
function getGlobal() {
  if (!gGlobalEnvironment) {
    gGlobalEnvironment = new EnvironmentCache();
  }
  return gGlobalEnvironment;
}

this.TelemetryEnvironment = {
  get currentEnvironment() {
    return getGlobal().currentEnvironment;
  },

  onInitialized: function() {
    return getGlobal().onInitialized();
  },

  registerChangeListener: function(name, listener) {
    return getGlobal().registerChangeListener(name, listener);
  },

  unregisterChangeListener: function(name) {
    return getGlobal().unregisterChangeListener(name);
  },

  // Policy to use when saving preferences. Exported for using them in tests.
  RECORD_PREF_STATE: 1, // Don't record the preference value
  RECORD_PREF_VALUE: 2, // We only record user-set prefs.
  RECORD_PREF_NOTIFY_ONLY: 3, // Record nothing, just notify of changes.

  // Testing method
  _watchPreferences: function(prefMap) {
    return getGlobal()._watchPreferences(prefMap);
  },
};

const DEFAULT_ENVIRONMENT_PREFS = new Map([
  ["app.feedback.baseURL", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.support.baseURL", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["accessibility.browsewithcaret", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["accessibility.force_disabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.auto", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.interval", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.service.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.silent", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["app.update.url", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.cache.disk.enable", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.cache.disk.capacity", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.cache.memory.enable", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.cache.offline.enable", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.formfill.enable", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.newtab.url", TelemetryEnvironment.RECORD_PREF_STATE],
  ["browser.newtabpage.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.newtabpage.enhanced", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.polaris.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.shell.checkDefaultBrowser", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["browser.startup.homepage", TelemetryEnvironment.RECORD_PREF_STATE],
  ["browser.startup.page", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["devtools.chrome.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["devtools.debugger.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["devtools.debugger.remote-enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["dom.ipc.plugins.asyncInit", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["dom.ipc.plugins.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["dom.ipc.processCount", TelemetryEnviroment.RECORD_PREF_VALUE],
  ["experiments.manifest.uri", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.blocklist.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.blocklist.url", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.strictCompatibility", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.update.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.update.url", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["extensions.update.background.url", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["general.smoothScroll", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["gfx.direct2d.disabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["gfx.direct2d.force-enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["gfx.direct2d.use1_1", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.acceleration.disabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.acceleration.force-enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.async-pan-zoom.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.async-video-oop.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.async-video.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.componentalpha.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.d3d11.disable-warp", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.d3d11.force-warp", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.offmainthreadcomposition.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.prefer-d3d9", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layers.prefer-opengl", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["layout.css.devPixelsPerPx", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["network.proxy.autoconfig_url", TelemetryEnvironment.RECORD_PREF_STATE],
  ["network.proxy.http", TelemetryEnvironment.RECORD_PREF_STATE],
  ["network.proxy.ssl", TelemetryEnvironment.RECORD_PREF_STATE],
  ["pdfjs.disabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["places.history.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["privacy.trackingprotection.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["privacy.donottrackheader.enabled", TelemetryEnvironment.RECORD_PREF_VALUE],
  ["services.sync.serverURL", TelemetryEnvironment.RECORD_PREF_STATE],
]);

const LOGGER_NAME = "Toolkit.Telemetry";

const PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
const PREF_DISTRIBUTION_ID = "distribution.id";
const PREF_DISTRIBUTION_VERSION = "distribution.version";
const PREF_DISTRIBUTOR = "app.distributor";
const PREF_DISTRIBUTOR_CHANNEL = "app.distributor.channel";
const PREF_HOTFIX_LASTVERSION = "extensions.hotfix.lastVersion";
const PREF_APP_PARTNER_BRANCH = "app.partner.";
const PREF_PARTNER_ID = "mozilla.partner.id";
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
const PREF_UPDATE_ENABLED = "app.update.enabled";
const PREF_UPDATE_AUTODOWNLOAD = "app.update.auto";
const PREF_SEARCH_COHORT = "browser.search.cohort";

const EXPERIMENTS_CHANGED_TOPIC = "experiments-changed";
const SEARCH_ENGINE_MODIFIED_TOPIC = "browser-search-engine-modified";
const SEARCH_SERVICE_TOPIC = "browser-search-service";
const COMPOSITOR_CREATED_TOPIC = "compositor:created";

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
  // Note that gfxInfo, and so getGfxField, might return "Unknown" for the RAM on failures,
  // not null.
  let memoryMB = parseInt(getGfxField("adapterRAM" + aSuffix, null), 10);
  if (Number.isNaN(memoryMB)) {
    memoryMB = null;
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

/**
 * Encapsulates the asynchronous magic interfacing with the addon manager. The builder
 * is owned by a parent environment object and is an addon listener.
 */
function EnvironmentAddonBuilder(environment) {
  this._environment = environment;

  // The pending task blocks addon manager shutdown. It can either be the initial load
  // or a change load.
  this._pendingTask = null;

  // Set to true once initial load is complete and we're watching for changes.
  this._loaded = false;
}
EnvironmentAddonBuilder.prototype = {
  /**
   * Get the initial set of addons.
   * @returns Promise<void> when the initial load is complete.
   */
  init: function() {
    // Some tests don't initialize the addon manager. This accounts for the
    // unfortunate reality of life.
    try {
      AddonManager.shutdown.addBlocker("EnvironmentAddonBuilder",
        () => this._shutdownBlocker());
    } catch (err) {
      return Promise.reject(err);
    }

    this._pendingTask = this._updateAddons().then(
      () => { this._pendingTask = null; },
      (err) => {
        this._environment._log.error("init - Exception in _updateAddons", err);
        this._pendingTask = null;
      }
    );

    return this._pendingTask;
  },

  /**
   * Register an addon listener and watch for changes.
   */
  watchForChanges: function() {
    this._loaded = true;
    AddonManager.addAddonListener(this);
    Services.obs.addObserver(this, EXPERIMENTS_CHANGED_TOPIC, false);
  },

  // AddonListener
  onEnabled: function() {
    this._onAddonChange();
  },
  onDisabled: function() {
    this._onAddonChange();
  },
  onInstalled: function() {
    this._onAddonChange();
  },
  onUninstalling: function() {
    this._onAddonChange();
  },

  _onAddonChange: function() {
    this._environment._log.trace("_onAddonChange");
    this._checkForChanges("addons-changed");
  },

  // nsIObserver
  observe: function (aSubject, aTopic, aData) {
    this._environment._log.trace("observe - Topic " + aTopic);
    this._checkForChanges("experiment-changed");
  },

  _checkForChanges: function(changeReason) {
    if (this._pendingTask) {
      this._environment._log.trace("_checkForChanges - task already pending, dropping change with reason " + changeReason);
      return;
    }

    this._pendingTask = this._updateAddons().then(
      (result) => {
        this._pendingTask = null;
        if (result.changed) {
          this._environment._onEnvironmentChange(changeReason, result.oldEnvironment);
        }
      },
      (err) => {
        this._pendingTask = null;
        this._environment._log.error("_checkForChanges: Error collecting addons", err);
      });
  },

  _shutdownBlocker: function() {
    if (this._loaded) {
      AddonManager.removeAddonListener(this);
      Services.obs.removeObserver(this, EXPERIMENTS_CHANGED_TOPIC);
    }
    return this._pendingTask;
  },

  /**
   * Collect the addon data for the environment.
   *
   * This should only be called from _pendingTask; otherwise we risk
   * running this during addon manager shutdown.
   *
   * @returns Promise<Object> This returns a Promise resolved with a status object with the following members:
   *   changed - Whether the environment changed.
   *   oldEnvironment - Only set if a change occured, contains the environment data before the change.
   */
  _updateAddons: Task.async(function* () {
    this._environment._log.trace("_updateAddons");
    let personaId = null;
#ifndef MOZ_WIDGET_GONK
    let theme = LightweightThemeManager.currentTheme;
    if (theme) {
      personaId = theme.id;
    }
#endif

    let addons = {
      activeAddons: yield this._getActiveAddons(),
      theme: yield this._getActiveTheme(),
      activePlugins: this._getActivePlugins(),
      activeGMPlugins: yield this._getActiveGMPlugins(),
      activeExperiment: this._getActiveExperiment(),
      persona: personaId,
    };

    let result = {
      changed: !ObjectUtils.deepEqual(addons, this._environment._currentEnvironment.addons),
    };

    if (result.changed) {
      this._environment._log.trace("_updateAddons: addons differ");
      result.oldEnvironment = Cu.cloneInto(this._environment._currentEnvironment, myScope);
      this._environment._currentEnvironment.addons = addons;
    }

    return result;
  }),

  /**
   * Get the addon data in object form.
   * @return Promise<object> containing the addon data.
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
        installDay: Utils.millisecondsToDays(installDate.getTime()),
        updateDay: Utils.millisecondsToDays(updateDate.getTime()),
        signedState: addon.signedState,
      };

      if (addon.signedState !== undefined)
        activeAddons[addon.id].signedState = addon.signedState;
    }

    return activeAddons;
  }),

  /**
   * Get the currently active theme data in object form.
   * @return Promise<object> containing the active theme data.
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
        installDay: Utils.millisecondsToDays(installDate.getTime()),
        updateDay: Utils.millisecondsToDays(updateDate.getTime()),
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
        updateDay: Utils.millisecondsToDays(updateDate.getTime()),
      });
    }

    return activePlugins;
  },

  /**
   * Get the GMPlugins data in object form.
   * @return Object containing the GMPlugins data.
   *
   * This should only be called from _pendingTask; otherwise we risk
   * running this during addon manager shutdown.
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
      let experiments = scope.Experiments.instance();
      let activeExperiment = experiments.getActiveExperimentID();
      if (activeExperiment) {
        experimentInfo.id = activeExperiment;
        experimentInfo.branch = experiments.getActiveExperimentBranch();
      }
    } catch(e) {
      // If this is not Firefox, the import will fail.
    }

    return experimentInfo;
  },
};

function EnvironmentCache() {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    LOGGER_NAME, "TelemetryEnvironment::");
  this._log.trace("constructor");

  this._shutdown = false;

  // A map of listeners that will be called on environment changes.
  this._changeListeners = new Map();

  // The last change date for the environment, used to throttle environment changes.
  this._lastEnvironmentChangeDate = null;

  // A map of watched preferences which trigger an Environment change when
  // modified. Every entry contains a recording policy (RECORD_PREF_*).
  this._watchedPrefs = DEFAULT_ENVIRONMENT_PREFS;

  this._currentEnvironment = {
    build: this._getBuild(),
    partner: this._getPartner(),
    system: this._getSystem(),
  };

  this._updateSettings();
  // Fill in the default search engine, if the search provider is already initialized.
  this._updateSearchEngine();

  // Build the remaining asynchronous parts of the environment. Don't register change listeners
  // until the initial environment has been built.

#ifdef MOZ_WIDGET_GONK
  this._addonBuilder = {
    watchForChanges: function() {}
  }
  let p = [];
#else
  this._addonBuilder = new EnvironmentAddonBuilder(this);

  let p = [ this._addonBuilder.init() ];
#endif
#ifndef MOZ_WIDGET_ANDROID
  this._currentEnvironment.profile = {};
  p.push(this._updateProfile());
#endif

  let setup = () => {
    this._initTask = null;
    this._startWatchingPrefs();
    this._addonBuilder.watchForChanges();
    this._addObservers();
    return this.currentEnvironment;
  };

  this._initTask = Promise.all(p)
    .then(
      () => setup(),
      (err) => {
        // log errors but eat them for consumers
        this._log.error("EnvironmentCache - error while initializing", err);
        return setup();
      });
}
EnvironmentCache.prototype = {
  /**
   * The current environment data. The returned data is cloned to avoid
   * unexpected sharing or mutation.
   * @returns object
   */
  get currentEnvironment() {
    return Cu.cloneInto(this._currentEnvironment, myScope);
  },

  /**
   * Wait for the current enviroment to be fully initialized.
   * @returns Promise<object>
   */
  onInitialized: function() {
    if (this._initTask) {
      return this._initTask;
    }
    return Promise.resolve(this.currentEnvironment);
  },

  /**
   * Register a listener for environment changes.
   * @param name The name of the listener. If a new listener is registered
   *             with the same name, the old listener will be replaced.
   * @param listener function(reason, oldEnvironment) - Will receive a reason for
                     the change and the environment data before the change.
   */
  registerChangeListener: function (name, listener) {
    this._log.trace("registerChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown");
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
    this._log.trace("unregisterChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown");
      return;
    }
    this._changeListeners.delete(name);
  },

  /**
   * Only used in tests, set the preferences to watch.
   * @param aPreferences A map of preferences names and their recording policy.
   */
  _watchPreferences: function (aPreferences) {
    this._stopWatchingPrefs();
    this._watchedPrefs = aPreferences;
    this._updateSettings();
    this._startWatchingPrefs();
  },

  /**
   * Get an object containing the values for the watched preferences. Depending on the
   * policy, the value for a preference or whether it was changed by user is reported.
   *
   * @return An object containing the preferences values.
   */
  _getPrefData: function () {
    let prefData = {};
    for (let [pref, policy] of this._watchedPrefs.entries()) {
      // Only record preferences if they are non-default and policy allows recording.
      if (!Preferences.isSet(pref) ||
          policy == TelemetryEnvironment.RECORD_PREF_NOTIFY_ONLY) {
        continue;
      }

      // Check the policy for the preference and decide if we need to store its value
      // or whether it changed from the default value.
      let prefValue = undefined;
      if (policy == TelemetryEnvironment.RECORD_PREF_STATE) {
        prefValue = "<user-set>";
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

    for (let pref of this._watchedPrefs.keys()) {
      Preferences.observe(pref, this._onPrefChanged, this);
    }
  },

  _onPrefChanged: function() {
    this._log.trace("_onPrefChanged");
    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    this._updateSettings();
    this._onEnvironmentChange("pref-changed", oldEnvironment);
  },

  /**
   * Do not receive any more change notifications for the preferences.
   */
  _stopWatchingPrefs: function () {
    this._log.trace("_stopWatchingPrefs");

    for (let pref of this._watchedPrefs.keys()) {
      Preferences.ignore(pref, this._onPrefChanged, this);
    }
  },

  _addObservers: function () {
    // Watch the search engine change and service topics.
    Services.obs.addObserver(this, SEARCH_ENGINE_MODIFIED_TOPIC, false);
    Services.obs.addObserver(this, SEARCH_SERVICE_TOPIC, false);
    Services.obs.addObserver(this, COMPOSITOR_CREATED_TOPIC, false);
  },

  _removeObservers: function () {
    // Remove the search engine change and service observers.
    Services.obs.removeObserver(this, SEARCH_ENGINE_MODIFIED_TOPIC);
    Services.obs.removeObserver(this, SEARCH_SERVICE_TOPIC);
    Services.obs.removeObserver(this, COMPOSITOR_CREATED_TOPIC);
  },

  observe: function (aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic + ", aData: " + aData);
    switch (aTopic) {
      case SEARCH_ENGINE_MODIFIED_TOPIC:
        if (aData != "engine-default" && aData != "engine-current") {
          return;
        }
        // Record the new default search choice and send the change notification.
        this._onSearchEngineChange();
        break;
      case SEARCH_SERVICE_TOPIC:
        if (aData != "init-complete") {
          return;
        }
        // Now that the search engine init is complete, record the default search choice.
        this._updateSearchEngine();
        break;
      case COMPOSITOR_CREATED_TOPIC:
        // Full graphics information is not available until we have created at
        // least one off-main-thread-composited window. Thus we wait for the
        // first compositor to be created and then query nsIGfxInfo again.
        this._onCompositorCreated();
        break;
    }
  },

  /**
   * Get the default search engine.
   * @return {String} Returns the search engine identifier, "NONE" if no default search
   *         engine is defined or "UNDEFINED" if no engine identifier or name can be found.
   */
  _getDefaultSearchEngine: function () {
    let engine;
    try {
      engine = Services.search.defaultEngine;
    } catch (e) {}

    let name;
    if (!engine) {
      name = "NONE";
    } else if (engine.identifier) {
      name = engine.identifier;
    } else if (engine.name) {
      name = "other-" + engine.name;
    } else {
      name = "UNDEFINED";
    }

    return name;
  },

  /**
   * Update the default search engine value.
   */
  _updateSearchEngine: function () {
    if (!Services.search) {
      // Just ignore cases where the search service is not implemented.
      return;
    }

    this._log.trace("_updateSearchEngine - isInitialized: " + Services.search.isInitialized);
    if (!Services.search.isInitialized) {
      return;
    }

    // Make sure we have a settings section.
    this._currentEnvironment.settings = this._currentEnvironment.settings || {};
    // Update the search engine entry in the current environment.
    this._currentEnvironment.settings.defaultSearchEngine = this._getDefaultSearchEngine();
    this._currentEnvironment.settings.defaultSearchEngineData =
      Services.search.getDefaultEngineInfo();

    // Record the cohort identifier used for search defaults A/B testing.
    if (Services.prefs.prefHasUserValue(PREF_SEARCH_COHORT))
      this._currentEnvironment.settings.searchCohort = Services.prefs.getCharPref(PREF_SEARCH_COHORT);
  },

  /**
   * Update the default search engine value and trigger the environment change.
   */
  _onSearchEngineChange: function () {
    this._log.trace("_onSearchEngineChange");

    // Finally trigger the environment change notification.
    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    this._updateSearchEngine();
    this._onEnvironmentChange("search-engine-changed", oldEnvironment);
  },

  /**
   * Update the graphics features object.
   */
  _onCompositorCreated: function () {
    let gfxData = this._currentEnvironment.system.gfx;
    try {
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
      gfxData.features = gfxInfo.getFeatures();
    } catch (e) {
      this._log.error("nsIGfxInfo.getFeatures() caught error", e);
    }
  },

  /**
   * Get the build data in object form.
   * @return Object containing the build data.
   */
  _getBuild: function () {
    let buildData = {
      applicationId: Services.appinfo.ID || null,
      applicationName: Services.appinfo.name || null,
      architecture: Services.sysinfo.get("arch"),
      buildId: Services.appinfo.appBuildID || null,
      version: Services.appinfo.version || null,
      vendor: Services.appinfo.vendor || null,
      platformVersion: Services.appinfo.platformVersion || null,
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
   * Determine if we're the default browser.
   * @returns null on error, true if we are the default browser, or false otherwise.
   */
  _isDefaultBrowser: function () {
#ifdef MOZ_WIDGET_GONK
    return true;
#else
    if (!("@mozilla.org/browser/shell-service;1" in Cc)) {
      this._log.info("_isDefaultBrowser - Could not obtain browser shell service");
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

    try {
      // This uses the same set of flags used by the pref pane.
      return shellService.isDefaultBrowser(false, true) ? true : false;
    } catch (ex) {
      this._log.error("_isDefaultBrowser - Could not determine if default browser", ex);
      return null;
    }

    return null;
#endif
  },

  /**
   * Update the cached settings data.
   */
  _updateSettings: function () {
    let updateChannel = null;
    try {
      updateChannel = UpdateChannel.get(false);
    } catch (e) {}

    this._currentEnvironment.settings = {
      blocklistEnabled: Preferences.get(PREF_BLOCKLIST_ENABLED, true),
#ifndef MOZ_WIDGET_ANDROID
      isDefaultBrowser: this._isDefaultBrowser(),
#endif
      e10sEnabled: Services.appinfo.browserTabsRemoteAutostart,
      telemetryEnabled: Preferences.get(PREF_TELEMETRY_ENABLED, false),
      locale: getBrowserLocale(),
      update: {
        channel: updateChannel,
        enabled: Preferences.get(PREF_UPDATE_ENABLED, true),
        autoDownload: Preferences.get(PREF_UPDATE_AUTODOWNLOAD, true),
      },
      userPrefs: this._getPrefData(),
    };

    this._updateSearchEngine();
  },

  /**
   * Update the cached profile data.
   * @returns Promise<> resolved when the I/O is complete.
   */
  _updateProfile: Task.async(function* () {
    const logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, "ProfileAge - ");
    let profileAccessor = new ProfileAge(null, logger);

    let creationDate = yield profileAccessor.created;
    let resetDate = yield profileAccessor.reset;

    this._currentEnvironment.profile.creationDate =
      Utils.millisecondsToDays(creationDate);
    if (resetDate) {
      this._currentEnvironment.profile.resetDate =
        Utils.millisecondsToDays(resetDate);
    }
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
    partnerData.partnerNames = partnerBranch.getChildList("");

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
      if (getSysinfoProperty(ext, false)) {
        availableExts.push(ext);
      }
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
      installYear: getSysinfoProperty("installYear", null),
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
      // The following line is disabled due to main thread jank and will be enabled
      // again as part of bug 1154500.
      //DWriteVersion: getGfxField("DWriteVersion", null),
      adapters: [],
      monitors: [],
      features: {},
    };

#if !defined(MOZ_WIDGET_GONK) && !defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_WIDGET_GTK)
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    try {
      gfxData.monitors = gfxInfo.getMonitors();
    } catch (e) {
      this._log.error("nsIGfxInfo.getMonitors() caught error", e);
    }
#endif

    try {
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
      gfxData.features = gfxInfo.getFeatures();
    } catch (e) {
      this._log.error("nsIGfxInfo.getFeatures() caught error", e);
    }

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

  _onEnvironmentChange: function (what, oldEnvironment) {
    this._log.trace("_onEnvironmentChange for " + what);
    if (this._shutdown) {
      this._log.trace("_onEnvironmentChange - Already shut down.");
      return;
    }

    // We are already skipping change events in _checkChanges if there is a pending change task running.
    let now = Policy.now();
    if (this._lastEnvironmentChangeDate &&
        (CHANGE_THROTTLE_INTERVAL_MS >=
         (now.getTime() - this._lastEnvironmentChangeDate.getTime()))) {
      this._log.trace("_onEnvironmentChange - throttling changes, now: " + now +
                      ", last change: " + this._lastEnvironmentChangeDate);
      return;
    }

    this._lastEnvironmentChangeDate = now;

    for (let [name, listener] of this._changeListeners) {
      try {
        this._log.debug("_onEnvironmentChange - calling " + name);
        listener(what, oldEnvironment);
      } catch (e) {
        this._log.error("_onEnvironmentChange - listener " + name + " caught error", e);
      }
    }
  },
};
