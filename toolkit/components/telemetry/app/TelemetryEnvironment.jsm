/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryEnvironment", "Policy"];

const myScope = this;

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const Utils = TelemetryUtils;

const { AddonManager, AddonManagerPrivate } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AttributionCode",
  "resource:///modules/AttributionCode.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WindowsRegistry",
  "resource://gre/modules/WindowsRegistry.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "WindowsVersionInfo",
  "resource://gre/modules/components-utils/WindowsVersionInfo.jsm"
);

// The maximum length of a string (e.g. description) in the addons section.
const MAX_ADDON_STRING_LENGTH = 100;
// The maximum length of a string value in the settings.attribution object.
const MAX_ATTRIBUTION_STRING_LENGTH = 100;
// The maximum lengths for the experiment id and branch in the experiments section.
const MAX_EXPERIMENT_ID_LENGTH = 100;
const MAX_EXPERIMENT_BRANCH_LENGTH = 100;
const MAX_EXPERIMENT_TYPE_LENGTH = 20;
const MAX_EXPERIMENT_ENROLLMENT_ID_LENGTH = 40;

/**
 * This is a policy object used to override behavior for testing.
 */
// eslint-disable-next-line no-unused-vars
var Policy = {
  now: () => new Date(),
  _intlLoaded: false,
  _browserDelayedStartup() {
    if (Policy._intlLoaded) {
      return Promise.resolve();
    }
    return new Promise(resolve => {
      let startupTopic = "browser-delayed-startup-finished";
      Services.obs.addObserver(function observer(subject, topic) {
        if (topic == startupTopic) {
          Services.obs.removeObserver(observer, startupTopic);
          resolve();
        }
      }, startupTopic);
    });
  },
};

// This is used to buffer calls to setExperimentActive and friends, so that we
// don't prematurely initialize our environment if it is called early during
// startup.
var gActiveExperimentStartupBuffer = new Map();

var gGlobalEnvironment;
function getGlobal() {
  if (!gGlobalEnvironment) {
    gGlobalEnvironment = new EnvironmentCache();
  }
  return gGlobalEnvironment;
}

var TelemetryEnvironment = {
  get currentEnvironment() {
    return getGlobal().currentEnvironment;
  },

  onInitialized() {
    return getGlobal().onInitialized();
  },

  delayedInit() {
    return getGlobal().delayedInit();
  },

  registerChangeListener(name, listener) {
    return getGlobal().registerChangeListener(name, listener);
  },

  unregisterChangeListener(name) {
    return getGlobal().unregisterChangeListener(name);
  },

  /**
   * Add an experiment annotation to the environment.
   * If an annotation with the same id already exists, it will be overwritten.
   * This triggers a new subsession, subject to throttling.
   *
   * @param {String} id The id of the active experiment.
   * @param {String} branch The experiment branch.
   * @param {Object} [options] Optional object with options.
   * @param {String} [options.type=false] The specific experiment type.
   * @param {String} [options.enrollmentId=undefined] The id of the enrollment.
   */
  setExperimentActive(id, branch, options = {}) {
    if (gGlobalEnvironment) {
      gGlobalEnvironment.setExperimentActive(id, branch, options);
    } else {
      gActiveExperimentStartupBuffer.set(id, { branch, options });
    }
  },

  /**
   * Remove an experiment annotation from the environment.
   * If the annotation exists, a new subsession will triggered.
   *
   * @param {String} id The id of the active experiment.
   */
  setExperimentInactive(id) {
    if (gGlobalEnvironment) {
      gGlobalEnvironment.setExperimentInactive(id);
    } else {
      gActiveExperimentStartupBuffer.delete(id);
    }
  },

  /**
   * Returns an object containing the data for the active experiments.
   *
   * The returned object is of the format:
   *
   * {
   *   "<experiment id>": { branch: "<branch>" },
   *   // …
   * }
   */
  getActiveExperiments() {
    if (gGlobalEnvironment) {
      return gGlobalEnvironment.getActiveExperiments();
    }

    const result = {};
    for (const [id, { branch }] of gActiveExperimentStartupBuffer.entries()) {
      result[id] = branch;
    }
    return result;
  },

  shutdown() {
    return getGlobal().shutdown();
  },

  // Policy to use when saving preferences. Exported for using them in tests.
  // Reports "<user-set>" if there is a value set on the user branch
  RECORD_PREF_STATE: 1,

  // Reports the value set on the user branch, if one is set
  RECORD_PREF_VALUE: 2,

  // Reports the active value (set on either the user or default branch)
  // for this pref, if one is set
  RECORD_DEFAULTPREF_VALUE: 3,

  // Reports "<set>" if a value for this pref is defined on either the user
  // or default branch
  RECORD_DEFAULTPREF_STATE: 4,

  // Testing method
  async testWatchPreferences(prefMap) {
    return getGlobal()._watchPreferences(prefMap);
  },

  /**
   * Intended for use in tests only.
   *
   * In multiple tests we need a way to shut and re-start telemetry together
   * with TelemetryEnvironment. This is problematic due to the fact that
   * TelemetryEnvironment is a singleton. We, therefore, need this helper
   * method to be able to re-set TelemetryEnvironment.
   */
  testReset() {
    return getGlobal().reset();
  },

  /**
   * Intended for use in tests only.
   */
  testCleanRestart() {
    getGlobal().shutdown();
    gGlobalEnvironment = null;
    gActiveExperimentStartupBuffer = new Map();
    return getGlobal();
  },
};

const RECORD_PREF_STATE = TelemetryEnvironment.RECORD_PREF_STATE;
const RECORD_PREF_VALUE = TelemetryEnvironment.RECORD_PREF_VALUE;
const RECORD_DEFAULTPREF_VALUE = TelemetryEnvironment.RECORD_DEFAULTPREF_VALUE;
const RECORD_DEFAULTPREF_STATE = TelemetryEnvironment.RECORD_DEFAULTPREF_STATE;
const DEFAULT_ENVIRONMENT_PREFS = new Map([
  ["app.feedback.baseURL", { what: RECORD_PREF_VALUE }],
  ["app.support.baseURL", { what: RECORD_PREF_VALUE }],
  ["accessibility.browsewithcaret", { what: RECORD_PREF_VALUE }],
  ["accessibility.force_disabled", { what: RECORD_PREF_VALUE }],
  ["app.normandy.test-prefs.bool", { what: RECORD_PREF_VALUE }],
  ["app.normandy.test-prefs.integer", { what: RECORD_PREF_VALUE }],
  ["app.normandy.test-prefs.string", { what: RECORD_PREF_VALUE }],
  ["app.shield.optoutstudies.enabled", { what: RECORD_PREF_VALUE }],
  ["app.update.interval", { what: RECORD_PREF_VALUE }],
  ["app.update.service.enabled", { what: RECORD_PREF_VALUE }],
  ["app.update.silent", { what: RECORD_PREF_VALUE }],
  ["browser.cache.disk.enable", { what: RECORD_PREF_VALUE }],
  ["browser.cache.disk.capacity", { what: RECORD_PREF_VALUE }],
  ["browser.cache.memory.enable", { what: RECORD_PREF_VALUE }],
  ["browser.cache.offline.enable", { what: RECORD_PREF_VALUE }],
  ["browser.formfill.enable", { what: RECORD_PREF_VALUE }],
  ["browser.newtabpage.enabled", { what: RECORD_PREF_VALUE }],
  ["browser.shell.checkDefaultBrowser", { what: RECORD_PREF_VALUE }],
  ["browser.search.ignoredJAREngines", { what: RECORD_DEFAULTPREF_VALUE }],
  ["browser.search.region", { what: RECORD_PREF_VALUE }],
  ["browser.search.suggest.enabled", { what: RECORD_PREF_VALUE }],
  ["browser.search.widget.inNavBar", { what: RECORD_DEFAULTPREF_VALUE }],
  ["browser.startup.homepage", { what: RECORD_PREF_STATE }],
  ["browser.startup.page", { what: RECORD_PREF_VALUE }],
  [
    "browser.urlbar.quicksuggest.onboardingDialogChoice",
    { what: RECORD_DEFAULTPREF_VALUE },
  ],
  [
    "browser.urlbar.quicksuggest.dataCollection.enabled",
    { what: RECORD_DEFAULTPREF_VALUE },
  ],
  ["browser.urlbar.showSearchSuggestionsFirst", { what: RECORD_PREF_VALUE }],
  [
    "browser.urlbar.suggest.quicksuggest.nonsponsored",
    { what: RECORD_DEFAULTPREF_VALUE },
  ],
  [
    "browser.urlbar.suggest.quicksuggest.sponsored",
    { what: RECORD_DEFAULTPREF_VALUE },
  ],
  ["browser.urlbar.suggest.searches", { what: RECORD_PREF_VALUE }],
  ["devtools.chrome.enabled", { what: RECORD_PREF_VALUE }],
  ["devtools.debugger.enabled", { what: RECORD_PREF_VALUE }],
  ["devtools.debugger.remote-enabled", { what: RECORD_PREF_VALUE }],
  ["doh-rollout.doorhanger-decision", { what: RECORD_PREF_VALUE }],
  ["dom.ipc.plugins.enabled", { what: RECORD_PREF_VALUE }],
  ["dom.ipc.plugins.sandbox-level.flash", { what: RECORD_PREF_VALUE }],
  ["dom.ipc.processCount", { what: RECORD_PREF_VALUE }],
  ["dom.max_script_run_time", { what: RECORD_PREF_VALUE }],
  ["extensions.autoDisableScopes", { what: RECORD_PREF_VALUE }],
  ["extensions.enabledScopes", { what: RECORD_PREF_VALUE }],
  ["extensions.blocklist.enabled", { what: RECORD_PREF_VALUE }],
  ["extensions.formautofill.addresses.enabled", { what: RECORD_PREF_VALUE }],
  [
    "extensions.formautofill.addresses.capture.enabled",
    { what: RECORD_PREF_VALUE },
  ],
  ["extensions.formautofill.creditCards.enabled", { what: RECORD_PREF_VALUE }],
  [
    "extensions.formautofill.creditCards.available",
    { what: RECORD_PREF_VALUE },
  ],
  ["extensions.formautofill.creditCards.used", { what: RECORD_PREF_VALUE }],
  ["extensions.strictCompatibility", { what: RECORD_PREF_VALUE }],
  ["extensions.update.enabled", { what: RECORD_PREF_VALUE }],
  ["extensions.update.url", { what: RECORD_PREF_VALUE }],
  ["extensions.update.background.url", { what: RECORD_PREF_VALUE }],
  ["extensions.screenshots.disabled", { what: RECORD_PREF_VALUE }],
  ["general.config.filename", { what: RECORD_DEFAULTPREF_STATE }],
  ["general.smoothScroll", { what: RECORD_PREF_VALUE }],
  ["gfx.direct2d.disabled", { what: RECORD_PREF_VALUE }],
  ["gfx.direct2d.force-enabled", { what: RECORD_PREF_VALUE }],
  ["gfx.webrender.all", { what: RECORD_PREF_VALUE }],
  ["gfx.webrender.all.qualified", { what: RECORD_PREF_VALUE }],
  ["layers.acceleration.disabled", { what: RECORD_PREF_VALUE }],
  ["layers.acceleration.force-enabled", { what: RECORD_PREF_VALUE }],
  ["layers.async-pan-zoom.enabled", { what: RECORD_PREF_VALUE }],
  ["layers.async-video-oop.enabled", { what: RECORD_PREF_VALUE }],
  ["layers.async-video.enabled", { what: RECORD_PREF_VALUE }],
  ["layers.d3d11.disable-warp", { what: RECORD_PREF_VALUE }],
  ["layers.d3d11.force-warp", { what: RECORD_PREF_VALUE }],
  [
    "layers.offmainthreadcomposition.force-disabled",
    { what: RECORD_PREF_VALUE },
  ],
  ["layers.prefer-d3d9", { what: RECORD_PREF_VALUE }],
  ["layers.prefer-opengl", { what: RECORD_PREF_VALUE }],
  ["layout.css.devPixelsPerPx", { what: RECORD_PREF_VALUE }],
  ["network.http.windows-sso.enabled", { what: RECORD_PREF_VALUE }],
  ["network.proxy.autoconfig_url", { what: RECORD_PREF_STATE }],
  ["network.proxy.http", { what: RECORD_PREF_STATE }],
  ["network.proxy.ssl", { what: RECORD_PREF_STATE }],
  ["network.trr.mode", { what: RECORD_PREF_VALUE }],
  ["network.trr.strict_native_fallback", { what: RECORD_PREF_VALUE }],
  ["pdfjs.disabled", { what: RECORD_PREF_VALUE }],
  ["places.history.enabled", { what: RECORD_PREF_VALUE }],
  ["plugins.show_infobar", { what: RECORD_PREF_VALUE }],
  ["privacy.firstparty.isolate", { what: RECORD_PREF_VALUE }],
  ["privacy.resistFingerprinting", { what: RECORD_PREF_VALUE }],
  ["privacy.trackingprotection.enabled", { what: RECORD_PREF_VALUE }],
  ["privacy.donottrackheader.enabled", { what: RECORD_PREF_VALUE }],
  ["security.enterprise_roots.auto-enabled", { what: RECORD_PREF_VALUE }],
  ["security.enterprise_roots.enabled", { what: RECORD_PREF_VALUE }],
  ["security.pki.mitm_detected", { what: RECORD_PREF_VALUE }],
  ["security.mixed_content.block_active_content", { what: RECORD_PREF_VALUE }],
  ["security.mixed_content.block_display_content", { what: RECORD_PREF_VALUE }],
  ["security.tls.version.enable-deprecated", { what: RECORD_PREF_VALUE }],
  ["signon.management.page.breach-alerts.enabled", { what: RECORD_PREF_VALUE }],
  ["signon.autofillForms", { what: RECORD_PREF_VALUE }],
  ["signon.generation.enabled", { what: RECORD_PREF_VALUE }],
  ["signon.rememberSignons", { what: RECORD_PREF_VALUE }],
  ["toolkit.telemetry.pioneerId", { what: RECORD_PREF_STATE }],
  ["widget.content.allow-gtk-dark-theme", { what: RECORD_DEFAULTPREF_VALUE }],
  ["widget.content.gtk-theme-override", { what: RECORD_PREF_STATE }],
  [
    "widget.content.gtk-high-contrast.enabled",
    { what: RECORD_DEFAULTPREF_VALUE },
  ],
  ["xpinstall.signatures.required", { what: RECORD_PREF_VALUE }],
  ["nimbus.debug", { what: RECORD_PREF_VALUE }],
]);

const LOGGER_NAME = "Toolkit.Telemetry";

const PREF_BLOCKLIST_ENABLED = "extensions.blocklist.enabled";
const PREF_DISTRIBUTION_ID = "distribution.id";
const PREF_DISTRIBUTION_VERSION = "distribution.version";
const PREF_DISTRIBUTOR = "app.distributor";
const PREF_DISTRIBUTOR_CHANNEL = "app.distributor.channel";
const PREF_APP_PARTNER_BRANCH = "app.partner.";
const PREF_PARTNER_ID = "mozilla.partner.id";

const COMPOSITOR_CREATED_TOPIC = "compositor:created";
const COMPOSITOR_PROCESS_ABORTED_TOPIC = "compositor:process-aborted";
const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";
const GFX_FEATURES_READY_TOPIC = "gfx-features-ready";
const SEARCH_ENGINE_MODIFIED_TOPIC = "browser-search-engine-modified";
const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SESSIONSTORE_WINDOWS_RESTORED_TOPIC = "sessionstore-windows-restored";
const PREF_CHANGED_TOPIC = "nsPref:changed";
const BLOCKLIST_LOADED_TOPIC = "plugin-blocklist-loaded";
const AUTO_UPDATE_PREF_CHANGE_TOPIC =
  UpdateUtils.PER_INSTALLATION_PREFS["app.update.auto"].observerTopic;
const BACKGROUND_UPDATE_PREF_CHANGE_TOPIC =
  UpdateUtils.PER_INSTALLATION_PREFS["app.update.background.enabled"]
    .observerTopic;
const SERVICES_INFO_CHANGE_TOPIC = "sync-ui-state:update";
const FIREFOX_SUGGEST_UPDATE_TOPIC = "firefox-suggest-update";

/**
 * Enforces the parameter to a boolean value.
 * @param aValue The input value.
 * @return {Boolean|Object} If aValue is a boolean or a number, returns its truthfulness
 *         value. Otherwise, return null.
 */
function enforceBoolean(aValue) {
  if (typeof aValue !== "number" && typeof aValue !== "boolean") {
    return null;
  }
  return Boolean(aValue);
}

/**
 * Get the current browser locale.
 * @return a string with the locale or null on failure.
 */
function getBrowserLocale() {
  try {
    return Services.locale.appLocaleAsBCP47;
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
    return Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    ).systemLocale;
  } catch (e) {
    return null;
  }
}

/**
 * Get the current OS locales.
 * @return an array of strings with the OS locales or null on failure.
 */
function getSystemLocales() {
  try {
    return Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    ).systemLocales;
  } catch (e) {
    return null;
  }
}

/**
 * Get the current OS regional preference locales.
 * @return an array of strings with the OS regional preference locales or null on failure.
 */
function getRegionalPrefsLocales() {
  try {
    return Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    ).regionalPrefsLocales;
  } catch (e) {
    return null;
  }
}

function getIntlSettings() {
  return {
    requestedLocales: Services.locale.requestedLocales,
    availableLocales: Services.locale.availableLocales,
    appLocales: Services.locale.appLocalesAsBCP47,
    systemLocales: getSystemLocales(),
    regionalPrefsLocales: getRegionalPrefsLocales(),
    acceptLanguages: Services.prefs
      .getComplexValue("intl.accept_languages", Ci.nsIPrefLocalizedString)
      .data.split(",")
      .map(str => str.trim()),
  };
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
    if (gfxProp !== undefined && gfxProp !== "") {
      return gfxProp;
    }
  } catch (e) {}

  return aDefault;
}

/**
 * Returns a substring of the input string.
 *
 * @param {String} aString The input string.
 * @param {Integer} aMaxLength The maximum length of the returned substring. If this is
 *        greater than the length of the input string, we return the whole input string.
 * @return {String} The substring or null if the input string is null.
 */
function limitStringToLength(aString, aMaxLength) {
  if (typeof aString !== "string") {
    return null;
  }
  return aString.substring(0, aMaxLength);
}

/**
 * Force a value to be a string.
 * Only if the value is null, null is returned instead.
 */
function forceToStringOrNull(aValue) {
  if (aValue === null) {
    return null;
  }

  return String(aValue);
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
    driverVendor: getGfxField("adapterDriverVendor" + aSuffix, null),
    driverVersion: getGfxField("adapterDriverVersion" + aSuffix, null),
    driverDate: getGfxField("adapterDriverDate" + aSuffix, null),
  };
}

/**
 * Encapsulates the asynchronous magic interfacing with the addon manager. The builder
 * is owned by a parent environment object and is an addon listener.
 */
function EnvironmentAddonBuilder(environment) {
  this._environment = environment;

  // The pending task blocks addon manager shutdown. It can either be the initial load
  // or a change load.
  this._pendingTask = null;

  // Have we added an observer to listen for blocklist changes that still needs to be
  // removed:
  this._blocklistObserverAdded = false;

  // Set to true once initial load is complete and we're watching for changes.
  this._loaded = false;

  // The state reported by the shutdown blocker if we hang shutdown.
  this._shutdownState = "Initial";
}
EnvironmentAddonBuilder.prototype = {
  /**
   * Get the initial set of addons.
   * @returns Promise<void> when the initial load is complete.
   */
  async init() {
    AddonManager.beforeShutdown.addBlocker(
      "EnvironmentAddonBuilder",
      () => this._shutdownBlocker(),
      { fetchState: () => this._shutdownState }
    );

    this._pendingTask = (async () => {
      try {
        this._shutdownState = "Awaiting _updateAddons";
        // Gather initial addons details
        await this._updateAddons(true);

        if (!this._environment._addonsAreFull) {
          // The addon database has not been loaded, wait for it to
          // initialize and gather full data as soon as it does.
          this._shutdownState = "Awaiting AddonManagerPrivate.databaseReady";
          await AddonManagerPrivate.databaseReady;

          // Now gather complete addons details.
          this._shutdownState = "Awaiting second _updateAddons";
          await this._updateAddons();
        }
      } catch (err) {
        this._environment._log.error("init - Exception in _updateAddons", err);
      } finally {
        this._pendingTask = null;
        this._shutdownState = "_pendingTask init complete. No longer blocking.";
      }
    })();

    return this._pendingTask;
  },

  /**
   * Register an addon listener and watch for changes.
   */
  watchForChanges() {
    this._loaded = true;
    AddonManager.addAddonListener(this);
  },

  // AddonListener
  onEnabled(addon) {
    this._onAddonChange(addon);
  },
  onDisabled(addon) {
    this._onAddonChange(addon);
  },
  onInstalled(addon) {
    this._onAddonChange(addon);
  },
  onUninstalling(addon) {
    this._onAddonChange(addon);
  },
  onUninstalled(addon) {
    this._onAddonChange(addon);
  },

  _onAddonChange(addon) {
    if (addon && addon.isBuiltin && !addon.isSystem) {
      return;
    }
    this._environment._log.trace("_onAddonChange");
    this._checkForChanges("addons-changed");
  },

  // nsIObserver
  observe(aSubject, aTopic, aData) {
    this._environment._log.trace("observe - Topic " + aTopic);
    if (aTopic == BLOCKLIST_LOADED_TOPIC) {
      Services.obs.removeObserver(this, BLOCKLIST_LOADED_TOPIC);
      this._blocklistObserverAdded = false;
      let gmpPluginsPromise = this._getActiveGMPlugins();
      gmpPluginsPromise.then(
        gmpPlugins => {
          let { addons } = this._environment._currentEnvironment;
          addons.activeGMPlugins = gmpPlugins;
        },
        err => {
          this._environment._log.error(
            "blocklist observe: Error collecting plugins",
            err
          );
        }
      );
    }
  },

  _checkForChanges(changeReason) {
    if (this._pendingTask) {
      this._environment._log.trace(
        "_checkForChanges - task already pending, dropping change with reason " +
          changeReason
      );
      return;
    }

    this._shutdownState = "_checkForChanges awaiting _updateAddons";
    this._pendingTask = this._updateAddons().then(
      result => {
        this._pendingTask = null;
        this._shutdownState = "No longer blocking, _updateAddons resolved";
        if (result.changed) {
          this._environment._onEnvironmentChange(
            changeReason,
            result.oldEnvironment
          );
        }
      },
      err => {
        this._pendingTask = null;
        this._shutdownState = "No longer blocking, _updateAddons rejected";
        this._environment._log.error(
          "_checkForChanges: Error collecting addons",
          err
        );
      }
    );
  },

  _shutdownBlocker() {
    if (this._loaded) {
      AddonManager.removeAddonListener(this);
      if (this._blocklistObserverAdded) {
        Services.obs.removeObserver(this, BLOCKLIST_LOADED_TOPIC);
      }
    }

    // At startup, _pendingTask is set to a Promise that does not resolve
    // until the addons database has been read so complete details about
    // addons are available.  Returning it here will cause it to block
    // profileBeforeChange, guranteeing that full information will be
    // available by the time profileBeforeChangeTelemetry is fired.
    return this._pendingTask;
  },

  /**
   * Collect the addon data for the environment.
   *
   * This should only be called from _pendingTask; otherwise we risk
   * running this during addon manager shutdown.
   *
   * @param {boolean} [atStartup]
   *        True if this is the first check we're performing at startup. In that
   *        situation, we defer some more expensive initialization.
   *
   * @returns Promise<Object> This returns a Promise resolved with a status object with the following members:
   *   changed - Whether the environment changed.
   *   oldEnvironment - Only set if a change occured, contains the environment data before the change.
   */
  async _updateAddons(atStartup) {
    this._environment._log.trace("_updateAddons");

    let addons = {
      activeAddons: await this._getActiveAddons(),
      theme: await this._getActiveTheme(),
      activeGMPlugins: await this._getActiveGMPlugins(atStartup),
    };

    let result = {
      changed:
        !this._environment._currentEnvironment.addons ||
        !ObjectUtils.deepEqual(
          addons.activeAddons,
          this._environment._currentEnvironment.addons.activeAddons
        ),
    };

    if (result.changed) {
      this._environment._log.trace("_updateAddons: addons differ");
      result.oldEnvironment = Cu.cloneInto(
        this._environment._currentEnvironment,
        myScope
      );
    }
    this._environment._currentEnvironment.addons = addons;

    return result;
  },

  /**
   * Get the addon data in object form.
   * @return Promise<object> containing the addon data.
   */
  async _getActiveAddons() {
    // Request addons, asynchronously.
    let { addons: allAddons, fullData } = await AddonManager.getActiveAddons([
      "extension",
      "service",
    ]);

    this._environment._addonsAreFull = fullData;
    let activeAddons = {};
    for (let addon of allAddons) {
      // Don't collect any information about the new built-in search webextensions
      if (addon.isBuiltin && !addon.isSystem) {
        continue;
      }
      // Weird addon data in the wild can lead to exceptions while collecting
      // the data.
      try {
        // Make sure to have valid dates.
        let updateDate = new Date(Math.max(0, addon.updateDate));

        activeAddons[addon.id] = {
          version: limitStringToLength(addon.version, MAX_ADDON_STRING_LENGTH),
          scope: addon.scope,
          type: addon.type,
          updateDay: Utils.millisecondsToDays(updateDate.getTime()),
          isSystem: addon.isSystem,
          isWebExtension: addon.isWebExtension,
          multiprocessCompatible: true,
        };

        // getActiveAddons() gives limited data during startup and full
        // data after the addons database is loaded.
        if (fullData) {
          let installDate = new Date(Math.max(0, addon.installDate));
          Object.assign(activeAddons[addon.id], {
            blocklisted:
              addon.blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
            description: limitStringToLength(
              addon.description,
              MAX_ADDON_STRING_LENGTH
            ),
            name: limitStringToLength(addon.name, MAX_ADDON_STRING_LENGTH),
            userDisabled: enforceBoolean(addon.userDisabled),
            appDisabled: addon.appDisabled,
            foreignInstall: enforceBoolean(addon.foreignInstall),
            hasBinaryComponents: false,
            installDay: Utils.millisecondsToDays(installDate.getTime()),
            signedState: addon.signedState,
          });
        }
      } catch (ex) {
        this._environment._log.error(
          "_getActiveAddons - An addon was discarded due to an error",
          ex
        );
        continue;
      }
    }

    return activeAddons;
  },

  /**
   * Get the currently active theme data in object form.
   * @return Promise<object> containing the active theme data.
   */
  async _getActiveTheme() {
    // Request themes, asynchronously.
    let { addons: themes } = await AddonManager.getActiveAddons(["theme"]);

    let activeTheme = {};
    // We only store information about the active theme.
    let theme = themes.find(theme => theme.isActive);
    if (theme) {
      // Make sure to have valid dates.
      let installDate = new Date(Math.max(0, theme.installDate));
      let updateDate = new Date(Math.max(0, theme.updateDate));

      activeTheme = {
        id: theme.id,
        blocklisted:
          theme.blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
        description: limitStringToLength(
          theme.description,
          MAX_ADDON_STRING_LENGTH
        ),
        name: limitStringToLength(theme.name, MAX_ADDON_STRING_LENGTH),
        userDisabled: enforceBoolean(theme.userDisabled),
        appDisabled: theme.appDisabled,
        version: limitStringToLength(theme.version, MAX_ADDON_STRING_LENGTH),
        scope: theme.scope,
        foreignInstall: enforceBoolean(theme.foreignInstall),
        hasBinaryComponents: false,
        installDay: Utils.millisecondsToDays(installDate.getTime()),
        updateDay: Utils.millisecondsToDays(updateDate.getTime()),
      };
    }

    return activeTheme;
  },

  /**
   * Get the GMPlugins data in object form.
   *
   * @param {boolean} [atStartup]
   *        True if this is the first check we're performing at startup. In that
   *        situation, we defer some more expensive initialization.
   *
   * @return Object containing the GMPlugins data.
   *
   * This should only be called from _pendingTask; otherwise we risk
   * running this during addon manager shutdown.
   */
  async _getActiveGMPlugins(atStartup) {
    // If we haven't yet loaded the blocklist, pass back dummy data for now,
    // and add an observer to update this data as soon as we get it.
    if (atStartup || !Services.blocklist.isLoaded) {
      if (!this._blocklistObserverAdded) {
        Services.obs.addObserver(this, BLOCKLIST_LOADED_TOPIC);
        this._blocklistObserverAdded = true;
      }
      return {
        "dummy-gmp": {
          version: "0.1",
          userDisabled: false,
          applyBackgroundUpdates: 1,
        },
      };
    }
    // Request plugins, asynchronously.
    let allPlugins = await AddonManager.getAddonsByTypes(["plugin"]);

    let activeGMPlugins = {};
    for (let plugin of allPlugins) {
      // Only get info for active GMplugins.
      if (!plugin.isGMPlugin || !plugin.isActive) {
        continue;
      }

      try {
        activeGMPlugins[plugin.id] = {
          version: plugin.version,
          userDisabled: enforceBoolean(plugin.userDisabled),
          applyBackgroundUpdates: plugin.applyBackgroundUpdates,
        };
      } catch (ex) {
        this._environment._log.error(
          "_getActiveGMPlugins - A GMPlugin was discarded due to an error",
          ex
        );
        continue;
      }
    }

    return activeGMPlugins;
  },
};

function EnvironmentCache() {
  this._log = Log.repository.getLoggerWithMessagePrefix(
    LOGGER_NAME,
    "TelemetryEnvironment::"
  );
  this._log.trace("constructor");

  this._shutdown = false;
  // Don't allow querying the search service too early to prevent
  // impacting the startup performance.
  this._canQuerySearch = false;
  // To guard against slowing down startup, defer gathering heavy environment
  // entries until the session is restored.
  this._sessionWasRestored = false;

  // A map of listeners that will be called on environment changes.
  this._changeListeners = new Map();

  // A map of watched preferences which trigger an Environment change when
  // modified. Every entry contains a recording policy (RECORD_PREF_*).
  this._watchedPrefs = DEFAULT_ENVIRONMENT_PREFS;

  this._currentEnvironment = {
    build: this._getBuild(),
    partner: this._getPartner(),
    system: this._getSystem(),
  };

  this._addObservers();

  // Build the remaining asynchronous parts of the environment. Don't register change listeners
  // until the initial environment has been built.

  let p = [this._updateSettings()];
  this._addonBuilder = new EnvironmentAddonBuilder(this);
  p.push(this._addonBuilder.init());

  this._currentEnvironment.profile = {};
  p.push(this._updateProfile());
  if (AppConstants.MOZ_BUILD_APP == "browser") {
    p.push(this._loadAttributionAsync());
  }
  p.push(this._loadAsyncUpdateSettings());
  p.push(this._loadIntlData());

  for (const [
    id,
    { branch, options },
  ] of gActiveExperimentStartupBuffer.entries()) {
    this.setExperimentActive(id, branch, options);
  }
  gActiveExperimentStartupBuffer = null;

  let setup = () => {
    this._initTask = null;
    this._startWatchingPrefs();
    this._addonBuilder.watchForChanges();
    this._updateGraphicsFeatures();
    return this.currentEnvironment;
  };

  this._initTask = Promise.all(p).then(
    () => setup(),
    err => {
      // log errors but eat them for consumers
      this._log.error("EnvironmentCache - error while initializing", err);
      return setup();
    }
  );

  // Addons may contain partial or full data depending on whether the Addons DB
  // has had a chance to load. Do we have full data yet?
  this._addonsAreFull = false;
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
  onInitialized() {
    if (this._initTask) {
      return this._initTask;
    }
    return Promise.resolve(this.currentEnvironment);
  },

  /**
   * This gets called when the delayed init completes.
   */
  async delayedInit() {
    this._processData = await Services.sysinfo.processInfo;
    let processData = await Services.sysinfo.processInfo;
    // Remove isWow64 and isWowARM64 from processData
    // to strip it down to just CPU info
    delete processData.isWow64;
    delete processData.isWowARM64;

    let oldEnv = null;
    if (!this._initTask) {
      oldEnv = this.currentEnvironment;
    }

    this._cpuData = this._getCPUData();
    // Augment the return value from the promises with cached values
    this._cpuData = { ...processData, ...this._cpuData };

    this._currentEnvironment.system.cpu = this._getCPUData();

    if (AppConstants.platform == "win") {
      this._hddData = await Services.sysinfo.diskInfo;
      let osData = await Services.sysinfo.osInfo;

      if (!this._initTask) {
        // We've finished creating the initial env, so notify for the update
        // This is all a bit awkward because `currentEnvironment` clones
        // the object, which we need to pass to the notification, but we
        // should only notify once we've updated the current environment...
        // Ideally, _onEnvironmentChange should somehow deal with all this
        // instead of all the consumers.
        oldEnv = this.currentEnvironment;
      }

      this._osData = this._getOSData();

      // Augment the return values from the promises with cached values
      this._osData = Object.assign(osData, this._osData);

      this._currentEnvironment.system.os = this._getOSData();
      this._currentEnvironment.system.hdd = this._getHDDData();

      // Windows only values stored in processData
      this._currentEnvironment.system.isWow64 = this._getProcessData().isWow64;
      this._currentEnvironment.system.isWowARM64 = this._getProcessData().isWowARM64;
    }

    if (!this._initTask) {
      this._onEnvironmentChange("system-info", oldEnv);
    }
  },

  /**
   * Register a listener for environment changes.
   * @param name The name of the listener. If a new listener is registered
   *             with the same name, the old listener will be replaced.
   * @param listener function(reason, oldEnvironment) - Will receive a reason for
                     the change and the environment data before the change.
   */
  registerChangeListener(name, listener) {
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
  unregisterChangeListener(name) {
    this._log.trace("unregisterChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown");
      return;
    }
    this._changeListeners.delete(name);
  },

  setExperimentActive(id, branch, options) {
    this._log.trace(`setExperimentActive - id: ${id}, branch: ${branch}`);
    // Make sure both the id and the branch have sane lengths.
    const saneId = limitStringToLength(id, MAX_EXPERIMENT_ID_LENGTH);
    const saneBranch = limitStringToLength(
      branch,
      MAX_EXPERIMENT_BRANCH_LENGTH
    );
    if (!saneId || !saneBranch) {
      this._log.error(
        "setExperimentActive - the provided arguments are not strings."
      );
      return;
    }

    // Warn the user about any content truncation.
    if (saneId.length != id.length || saneBranch.length != branch.length) {
      this._log.warn(
        "setExperimentActive - the experiment id or branch were truncated."
      );
    }

    // Truncate the experiment type if present.
    if (options.hasOwnProperty("type")) {
      let type = limitStringToLength(options.type, MAX_EXPERIMENT_TYPE_LENGTH);
      if (type.length != options.type.length) {
        options.type = type;
        this._log.warn(
          "setExperimentActive - the experiment type was truncated."
        );
      }
    }

    // Truncate the enrollment id if present.
    if (options.hasOwnProperty("enrollmentId")) {
      let enrollmentId = limitStringToLength(
        options.enrollmentId,
        MAX_EXPERIMENT_ENROLLMENT_ID_LENGTH
      );
      if (enrollmentId.length != options.enrollmentId.length) {
        options.enrollmentId = enrollmentId;
        this._log.warn(
          "setExperimentActive - the enrollment id was truncated."
        );
      }
    }

    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    // Add the experiment annotation.
    let experiments = this._currentEnvironment.experiments || {};
    experiments[saneId] = { branch: saneBranch };
    if (options.hasOwnProperty("type")) {
      experiments[saneId].type = options.type;
    }
    if (options.hasOwnProperty("enrollmentId")) {
      experiments[saneId].enrollmentId = options.enrollmentId;
    }
    this._currentEnvironment.experiments = experiments;
    // Notify of the change.
    this._onEnvironmentChange("experiment-annotation-changed", oldEnvironment);
  },

  setExperimentInactive(id) {
    this._log.trace("setExperimentInactive");
    let experiments = this._currentEnvironment.experiments || {};
    if (id in experiments) {
      // Only attempt to notify if a previous annotation was found and removed.
      let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
      // Remove the experiment annotation.
      delete this._currentEnvironment.experiments[id];
      // Notify of the change.
      this._onEnvironmentChange(
        "experiment-annotation-changed",
        oldEnvironment
      );
    }
  },

  getActiveExperiments() {
    return Cu.cloneInto(this._currentEnvironment.experiments || {}, myScope);
  },

  shutdown() {
    this._log.trace("shutdown");
    this._shutdown = true;
  },

  /**
   * Only used in tests, set the preferences to watch.
   * @param aPreferences A map of preferences names and their recording policy.
   */
  async _watchPreferences(aPreferences) {
    this._stopWatchingPrefs();
    this._watchedPrefs = aPreferences;
    await this._updateSettings();
    this._startWatchingPrefs();
  },

  /**
   * Get an object containing the values for the watched preferences. Depending on the
   * policy, the value for a preference or whether it was changed by user is reported.
   *
   * @return An object containing the preferences values.
   */
  _getPrefData() {
    let prefData = {};
    for (let [pref, policy] of this._watchedPrefs.entries()) {
      let prefValue = this._getPrefValue(pref, policy.what);

      if (prefValue === undefined) {
        continue;
      }

      prefData[pref] = prefValue;
    }
    return prefData;
  },

  /**
   * Get the value of a preference given the preference name and the policy.
   * @param pref Name of the preference.
   * @param what Policy of the preference.
   *
   * @returns The value we need to store for this preference. It can be undefined
   *          or null if the preference is invalid or has a value set by the user.
   */
  _getPrefValue(pref, what) {
    // Check the policy for the preference and decide if we need to store its value
    // or whether it changed from the default value.
    let prefType = Services.prefs.getPrefType(pref);

    if (
      what == TelemetryEnvironment.RECORD_DEFAULTPREF_VALUE ||
      what == TelemetryEnvironment.RECORD_DEFAULTPREF_STATE
    ) {
      // For default prefs, make sure they exist
      if (prefType == Ci.nsIPrefBranch.PREF_INVALID) {
        return undefined;
      }
    } else if (!Services.prefs.prefHasUserValue(pref)) {
      // For user prefs, make sure they are set
      return undefined;
    }

    if (what == TelemetryEnvironment.RECORD_DEFAULTPREF_STATE) {
      return "<set>";
    } else if (what == TelemetryEnvironment.RECORD_PREF_STATE) {
      return "<user-set>";
    } else if (prefType == Ci.nsIPrefBranch.PREF_STRING) {
      return Services.prefs.getStringPref(pref);
    } else if (prefType == Ci.nsIPrefBranch.PREF_BOOL) {
      return Services.prefs.getBoolPref(pref);
    } else if (prefType == Ci.nsIPrefBranch.PREF_INT) {
      return Services.prefs.getIntPref(pref);
    } else if (prefType == Ci.nsIPrefBranch.PREF_INVALID) {
      return null;
    }
    throw new Error(
      `Unexpected preference type ("${prefType}") for "${pref}".`
    );
  },

  QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"]),

  /**
   * Start watching the preferences.
   */
  _startWatchingPrefs() {
    this._log.trace("_startWatchingPrefs - " + this._watchedPrefs);

    Services.prefs.addObserver("", this, true);
  },

  _onPrefChanged(aData) {
    this._log.trace("_onPrefChanged");
    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    this._currentEnvironment.settings.userPrefs[aData] = this._getPrefValue(
      aData,
      this._watchedPrefs.get(aData).what
    );
    this._onEnvironmentChange("pref-changed", oldEnvironment);
  },

  /**
   * Do not receive any more change notifications for the preferences.
   */
  _stopWatchingPrefs() {
    this._log.trace("_stopWatchingPrefs");

    Services.prefs.removeObserver("", this);
  },

  _addObservers() {
    // Watch the search engine change and service topics.
    Services.obs.addObserver(this, SESSIONSTORE_WINDOWS_RESTORED_TOPIC);
    Services.obs.addObserver(this, COMPOSITOR_CREATED_TOPIC);
    Services.obs.addObserver(this, COMPOSITOR_PROCESS_ABORTED_TOPIC);
    Services.obs.addObserver(this, DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC);
    Services.obs.addObserver(this, GFX_FEATURES_READY_TOPIC);
    Services.obs.addObserver(this, SEARCH_ENGINE_MODIFIED_TOPIC);
    Services.obs.addObserver(this, SEARCH_SERVICE_TOPIC);
    Services.obs.addObserver(this, AUTO_UPDATE_PREF_CHANGE_TOPIC);
    Services.obs.addObserver(this, BACKGROUND_UPDATE_PREF_CHANGE_TOPIC);
    Services.obs.addObserver(this, SERVICES_INFO_CHANGE_TOPIC);
    Services.obs.addObserver(this, FIREFOX_SUGGEST_UPDATE_TOPIC);
  },

  _removeObservers() {
    Services.obs.removeObserver(this, SESSIONSTORE_WINDOWS_RESTORED_TOPIC);
    Services.obs.removeObserver(this, COMPOSITOR_CREATED_TOPIC);
    Services.obs.removeObserver(this, COMPOSITOR_PROCESS_ABORTED_TOPIC);
    try {
      Services.obs.removeObserver(
        this,
        DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC
      );
    } catch (ex) {}
    Services.obs.removeObserver(this, GFX_FEATURES_READY_TOPIC);
    Services.obs.removeObserver(this, SEARCH_ENGINE_MODIFIED_TOPIC);
    Services.obs.removeObserver(this, SEARCH_SERVICE_TOPIC);
    Services.obs.removeObserver(this, AUTO_UPDATE_PREF_CHANGE_TOPIC);
    Services.obs.removeObserver(this, BACKGROUND_UPDATE_PREF_CHANGE_TOPIC);
    Services.obs.removeObserver(this, SERVICES_INFO_CHANGE_TOPIC);
    Services.obs.removeObserver(this, FIREFOX_SUGGEST_UPDATE_TOPIC);
  },

  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic + ", aData: " + aData);
    switch (aTopic) {
      case SEARCH_ENGINE_MODIFIED_TOPIC:
        if (
          aData != "engine-default" &&
          aData != "engine-default-private" &&
          aData != "engine-changed"
        ) {
          return;
        }
        if (
          aData == "engine-changed" &&
          aSubject.QueryInterface(Ci.nsISearchEngine) &&
          Services.search.defaultEngine != aSubject
        ) {
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
        this._canQuerySearch = true;
        this._updateSearchEngine();
        break;
      case GFX_FEATURES_READY_TOPIC:
      case COMPOSITOR_CREATED_TOPIC:
        // Full graphics information is not available until we have created at
        // least one off-main-thread-composited window. Thus we wait for the
        // first compositor to be created and then query nsIGfxInfo again.
        this._updateGraphicsFeatures();
        break;
      case COMPOSITOR_PROCESS_ABORTED_TOPIC:
        // Our compositor process has been killed for whatever reason, so refresh
        // our reported graphics features and trigger an environment change.
        this._onCompositorProcessAborted();
        break;
      case DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC:
        // Distribution customizations are applied after final-ui-startup. query
        // partner prefs again when they are ready.
        this._updatePartner();
        Services.obs.removeObserver(this, aTopic);
        break;
      case SESSIONSTORE_WINDOWS_RESTORED_TOPIC:
        this._sessionWasRestored = true;
        // Make sure to initialize the search service once we've done restoring
        // the windows, so that we don't risk loosing search data.
        Services.search.init();
        // The default browser check could take some time, so just call it after
        // the session was restored.
        this._updateDefaultBrowser();
        break;
      case PREF_CHANGED_TOPIC:
        let options = this._watchedPrefs.get(aData);
        if (options && !options.requiresRestart) {
          this._onPrefChanged(aData);
        }
        break;
      case AUTO_UPDATE_PREF_CHANGE_TOPIC:
        this._currentEnvironment.settings.update.autoDownload = aData == "true";
        break;
      case BACKGROUND_UPDATE_PREF_CHANGE_TOPIC:
        this._currentEnvironment.settings.update.background = aData == "true";
        break;
      case SERVICES_INFO_CHANGE_TOPIC:
        this._updateServicesInfo();
        break;
      case FIREFOX_SUGGEST_UPDATE_TOPIC:
        this._updateFirefoxSuggest();
        break;
    }
  },

  /**
   * Update the default search engine value.
   */
  async _updateSearchEngine() {
    if (!this._canQuerySearch) {
      this._log.trace("_updateSearchEngine - ignoring early call");
      return;
    }

    this._log.trace(
      "_updateSearchEngine - isInitialized: " + Services.search.isInitialized
    );
    if (!Services.search.isInitialized) {
      return;
    }

    // Make sure we have a settings section.
    this._currentEnvironment.settings = this._currentEnvironment.settings || {};

    // Update the search engine entry in the current environment.
    const defaultEngineInfo = await Services.search.getDefaultEngineInfo();
    this._currentEnvironment.settings.defaultSearchEngine =
      defaultEngineInfo.defaultSearchEngine;
    this._currentEnvironment.settings.defaultSearchEngineData = {
      ...defaultEngineInfo.defaultSearchEngineData,
    };
    if ("defaultPrivateSearchEngine" in defaultEngineInfo) {
      this._currentEnvironment.settings.defaultPrivateSearchEngine =
        defaultEngineInfo.defaultPrivateSearchEngine;
    }
    if ("defaultPrivateSearchEngineData" in defaultEngineInfo) {
      this._currentEnvironment.settings.defaultPrivateSearchEngineData = {
        ...defaultEngineInfo.defaultPrivateSearchEngineData,
      };
    }
  },

  /**
   * Update the default search engine value and trigger the environment change.
   */
  async _onSearchEngineChange() {
    this._log.trace("_onSearchEngineChange");

    // Finally trigger the environment change notification.
    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    await this._updateSearchEngine();
    this._onEnvironmentChange("search-engine-changed", oldEnvironment);
  },

  /**
   * Refresh the Telemetry environment and trigger an environment change due to
   * a change in compositor process (normally this will mean we've fallen back
   * from out-of-process to in-process compositing).
   */
  _onCompositorProcessAborted() {
    this._log.trace("_onCompositorProcessAborted");

    // Trigger the environment change notification.
    let oldEnvironment = Cu.cloneInto(this._currentEnvironment, myScope);
    this._updateGraphicsFeatures();
    this._onEnvironmentChange("gfx-features-changed", oldEnvironment);
  },

  /**
   * Update the graphics features object.
   */
  _updateGraphicsFeatures() {
    let gfxData = this._currentEnvironment.system.gfx;
    try {
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
      gfxData.features = gfxInfo.getFeatures();
    } catch (e) {
      this._log.error("nsIGfxInfo.getFeatures() caught error", e);
    }
  },

  /**
   * Update the partner prefs.
   */
  _updatePartner() {
    this._currentEnvironment.partner = this._getPartner();
  },

  /**
   * Get the build data in object form.
   * @return Object containing the build data.
   */
  _getBuild() {
    let buildData = {
      applicationId: Services.appinfo.ID || null,
      applicationName: Services.appinfo.name || null,
      architecture: Services.sysinfo.get("arch"),
      buildId: Services.appinfo.appBuildID || null,
      version: Services.appinfo.version || null,
      vendor: Services.appinfo.vendor || null,
      displayVersion: AppConstants.MOZ_APP_VERSION_DISPLAY || null,
      platformVersion: Services.appinfo.platformVersion || null,
      xpcomAbi: Services.appinfo.XPCOMABI,
      updaterAvailable: AppConstants.MOZ_UPDATER,
    };

    return buildData;
  },

  /**
   * Determine if we're the default browser.
   * @returns null on error, true if we are the default browser, or false otherwise.
   */
  _isDefaultBrowser() {
    let isDefault = (service, ...args) => {
      try {
        return !!service.isDefaultBrowser(...args);
      } catch (ex) {
        this._log.error(
          "_isDefaultBrowser - Could not determine if default browser",
          ex
        );
        return null;
      }
    };

    if (!("@mozilla.org/browser/shell-service;1" in Cc)) {
      this._log.info(
        "_isDefaultBrowser - Could not obtain browser shell service"
      );
      return null;
    }

    try {
      let { ShellService } = ChromeUtils.import(
        "resource:///modules/ShellService.jsm"
      );
      // This uses the same set of flags used by the pref pane.
      return isDefault(ShellService, false, true);
    } catch (ex) {
      this._log.error("_isDefaultBrowser - Could not obtain shell service JSM");
    }

    try {
      let shellService = Cc["@mozilla.org/browser/shell-service;1"].getService(
        Ci.nsIShellService
      );
      // This uses the same set of flags used by the pref pane.
      return isDefault(shellService, true);
    } catch (ex) {
      this._log.error("_isDefaultBrowser - Could not obtain shell service", ex);
      return null;
    }
  },

  _updateDefaultBrowser() {
    if (AppConstants.platform === "android") {
      return;
    }
    // Make sure to have a settings section.
    this._currentEnvironment.settings = this._currentEnvironment.settings || {};
    this._currentEnvironment.settings.isDefaultBrowser = this
      ._sessionWasRestored
      ? this._isDefaultBrowser()
      : null;
  },

  /**
   * Update the cached settings data.
   */
  async _updateSettings() {
    let updateChannel = null;
    try {
      updateChannel = Utils.getUpdateChannel();
    } catch (e) {}

    this._currentEnvironment.settings = {
      blocklistEnabled: Services.prefs.getBoolPref(
        PREF_BLOCKLIST_ENABLED,
        true
      ),
      e10sEnabled: Services.appinfo.browserTabsRemoteAutostart,
      e10sMultiProcesses: Services.appinfo.maxWebProcessCount,
      fissionEnabled: Services.appinfo.fissionAutostart,
      telemetryEnabled: Utils.isTelemetryEnabled,
      locale: getBrowserLocale(),
      // We need to wait for browser-delayed-startup-finished to ensure that the locales
      // have settled, once that's happened we can get the intl data directly.
      intl: Policy._intlLoaded ? getIntlSettings() : {},
      update: {
        channel: updateChannel,
        enabled: !Services.policies || Services.policies.isAllowed("appUpdate"),
      },
      userPrefs: this._getPrefData(),
      sandbox: this._getSandboxData(),
    };

    // Services.appinfo.launcherProcessState is not available in all build
    // configurations, in which case an exception may be thrown.
    try {
      this._currentEnvironment.settings.launcherProcessState =
        Services.appinfo.launcherProcessState;
    } catch (e) {}

    this._currentEnvironment.settings.addonCompatibilityCheckEnabled =
      AddonManager.checkCompatibility;

    if (AppConstants.MOZ_BUILD_APP == "browser") {
      this._updateAttribution();
    }
    this._updateDefaultBrowser();
    await this._updateSearchEngine();
    this._loadAsyncUpdateSettingsFromCache();
  },

  _getSandboxData() {
    let effectiveContentProcessLevel = null;
    let contentWin32kLockdownState = null;
    try {
      let sandboxSettings = Cc[
        "@mozilla.org/sandbox/sandbox-settings;1"
      ].getService(Ci.mozISandboxSettings);
      effectiveContentProcessLevel =
        sandboxSettings.effectiveContentSandboxLevel;

      // See `ContentWin32kLockdownState` in
      // <security/sandbox/common/SandboxSettings.h>
      //
      // Values:
      // 1 = LockdownEnabled
      // 2 = MissingWebRender
      // 3 = OperatingSystemNotSupported
      // 4 = PrefNotSet
      contentWin32kLockdownState = sandboxSettings.contentWin32kLockdownState;
    } catch (e) {}
    return {
      effectiveContentProcessLevel,
      contentWin32kLockdownState,
    };
  },

  /**
   * Update the cached profile data.
   * @returns Promise<> resolved when the I/O is complete.
   */
  async _updateProfile() {
    let profileAccessor = await ProfileAge();

    let creationDate = await profileAccessor.created;
    let resetDate = await profileAccessor.reset;
    let firstUseDate = await profileAccessor.firstUse;

    this._currentEnvironment.profile.creationDate = Utils.millisecondsToDays(
      creationDate
    );
    if (resetDate) {
      this._currentEnvironment.profile.resetDate = Utils.millisecondsToDays(
        resetDate
      );
    }
    if (firstUseDate) {
      this._currentEnvironment.profile.firstUseDate = Utils.millisecondsToDays(
        firstUseDate
      );
    }
  },

  /**
   * Load the attribution data object and updates the environment.
   * @returns Promise<> resolved when the I/O is complete.
   */
  async _loadAttributionAsync() {
    try {
      await AttributionCode.getAttrDataAsync();
    } catch (e) {
      // The AttributionCode.jsm module might not be always available
      // (e.g. tests). Gracefully handle this.
      return;
    }
    this._updateAttribution();
  },

  /**
   * Update the environment with the cached attribution data.
   */
  _updateAttribution() {
    let data = null;
    try {
      data = AttributionCode.getCachedAttributionData();
    } catch (e) {
      // The AttributionCode.jsm module might not be always available
      // (e.g. tests). Gracefully handle this.
    }

    if (!data || !Object.keys(data).length) {
      return;
    }

    let attributionData = {};
    for (let key in data) {
      attributionData[key] = limitStringToLength(
        data[key],
        MAX_ATTRIBUTION_STRING_LENGTH
      );
    }
    this._currentEnvironment.settings.attribution = attributionData;
  },

  /**
   * Load the per-installation update settings, cache them, and add them to the
   * environment.
   */
  async _loadAsyncUpdateSettings() {
    if (AppConstants.MOZ_UPDATER) {
      this._updateAutoDownloadCache = await UpdateUtils.getAppUpdateAutoEnabled();
      this._updateBackgroundCache = await UpdateUtils.readUpdateConfigSetting(
        "app.update.background.enabled"
      );
    } else {
      this._updateAutoDownloadCache = false;
      this._updateBackgroundCache = false;
    }
    this._loadAsyncUpdateSettingsFromCache();
  },

  /**
   * Update the environment with the cached values for per-installation update
   * settings.
   */
  _loadAsyncUpdateSettingsFromCache() {
    if (this._updateAutoDownloadCache !== undefined) {
      this._currentEnvironment.settings.update.autoDownload = this._updateAutoDownloadCache;
    }
    if (this._updateBackgroundCache !== undefined) {
      this._currentEnvironment.settings.update.background = this._updateBackgroundCache;
    }
  },

  /**
   * Get i18n data about the system.
   * @return A promise of completion.
   */
  async _loadIntlData() {
    // Wait for the startup topic.
    await Policy._browserDelayedStartup();
    this._currentEnvironment.settings.intl = getIntlSettings();
    Policy._intlLoaded = true;
  },
  // This exists as a separate function for testing.
  async _getFxaSignedInUser() {
    return fxAccounts.getSignedInUser();
  },

  async _updateServicesInfo() {
    let syncEnabled = false;
    let accountEnabled = false;
    let weaveService = Cc["@mozilla.org/weave/service;1"].getService()
      .wrappedJSObject;
    syncEnabled = weaveService && weaveService.enabled;
    if (syncEnabled) {
      // All sync users are account users, definitely.
      accountEnabled = true;
    } else {
      // Not all account users are sync users. See if they're signed into FxA.
      try {
        let user = await this._getFxaSignedInUser();
        if (user) {
          accountEnabled = true;
        }
      } catch (e) {
        // We don't know. This might be a transient issue which will clear
        // itself up later, but the information in telemetry is quite possibly stale
        // (this is called from a change listener), so clear it out to avoid
        // reporting data which might be wrong until we can figure it out.
        delete this._currentEnvironment.services;
        this._log.error("_updateServicesInfo() caught error", e);
        return;
      }
    }
    this._currentEnvironment.services = {
      accountEnabled,
      syncEnabled,
    };
  },

  /**
   * Updates environment data related to Firefox Suggest.
   */
  _updateFirefoxSuggest() {
    let prefs = [
      "browser.urlbar.suggest.quicksuggest.nonsponsored",
      "browser.urlbar.suggest.quicksuggest.sponsored",
    ];
    for (let p of prefs) {
      this._currentEnvironment.settings.userPrefs[
        p
      ] = Services.prefs.getBoolPref(p);
    }
  },

  /**
   * Get the partner data in object form.
   * @return Object containing the partner data.
   */
  _getPartner() {
    let partnerData = {
      distributionId: Services.prefs.getStringPref(PREF_DISTRIBUTION_ID, null),
      distributionVersion: Services.prefs.getStringPref(
        PREF_DISTRIBUTION_VERSION,
        null
      ),
      partnerId: Services.prefs.getStringPref(PREF_PARTNER_ID, null),
      distributor: Services.prefs.getStringPref(PREF_DISTRIBUTOR, null),
      distributorChannel: Services.prefs.getStringPref(
        PREF_DISTRIBUTOR_CHANNEL,
        null
      ),
    };

    // Get the PREF_APP_PARTNER_BRANCH branch and append its children to partner data.
    let partnerBranch = Services.prefs.getBranch(PREF_APP_PARTNER_BRANCH);
    partnerData.partnerNames = partnerBranch.getChildList("");

    return partnerData;
  },

  _cpuData: null,
  /**
   * Get the CPU information.
   * @return Object containing the CPU information data.
   */
  _getCPUData() {
    if (this._cpuData) {
      return this._cpuData;
    }

    this._cpuData = {};

    const CPU_EXTENSIONS = [
      "hasMMX",
      "hasSSE",
      "hasSSE2",
      "hasSSE3",
      "hasSSSE3",
      "hasSSE4A",
      "hasSSE4_1",
      "hasSSE4_2",
      "hasAVX",
      "hasAVX2",
      "hasAES",
      "hasEDSP",
      "hasARMv6",
      "hasARMv7",
      "hasNEON",
      "hasUserCET",
    ];

    // Enumerate the available CPU extensions.
    let availableExts = [];
    for (let ext of CPU_EXTENSIONS) {
      if (getSysinfoProperty(ext, false)) {
        availableExts.push(ext);
      }
    }

    this._cpuData.extensions = availableExts;

    return this._cpuData;
  },

  _processData: null,
  /**
   * Get the process information.
   * @return Object containing the process information data.
   */
  _getProcessData() {
    if (this._processData) {
      return this._processData;
    }
    return {};
  },

  /**
   * Get the device information, if we are on a portable device.
   * @return Object containing the device information data, or null if
   * not a portable device.
   */
  _getDeviceData() {
    if (AppConstants.platform !== "android") {
      return null;
    }

    return {
      model: getSysinfoProperty("device", null),
      manufacturer: getSysinfoProperty("manufacturer", null),
      hardware: getSysinfoProperty("hardware", null),
      isTablet: getSysinfoProperty("tablet", null),
    };
  },

  _osData: null,
  /**
   * Get the OS information.
   * @return Object containing the OS data.
   */
  _getOSData() {
    if (this._osData) {
      return this._osData;
    }
    this._osData = {
      name: forceToStringOrNull(getSysinfoProperty("name", null)),
      version: forceToStringOrNull(getSysinfoProperty("version", null)),
      locale: forceToStringOrNull(getSystemLocale()),
    };

    if (AppConstants.platform == "android") {
      this._osData.kernelVersion = forceToStringOrNull(
        getSysinfoProperty("kernel_version", null)
      );
    } else if (AppConstants.platform === "win") {
      // The path to the "UBR" key, queried to get additional version details on Windows.
      const WINDOWS_UBR_KEY_PATH =
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

      let versionInfo = WindowsVersionInfo.get({ throwOnError: false });
      this._osData.servicePackMajor = versionInfo.servicePackMajor;
      this._osData.servicePackMinor = versionInfo.servicePackMinor;
      this._osData.windowsBuildNumber = versionInfo.buildNumber;
      // We only need the UBR if we're at or above Windows 10.
      if (
        typeof this._osData.version === "string" &&
        Services.vc.compare(this._osData.version, "10") >= 0
      ) {
        // Query the UBR key and only add it to the environment if it's available.
        // |readRegKey| doesn't throw, but rather returns 'undefined' on error.
        let ubr = WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
          WINDOWS_UBR_KEY_PATH,
          "UBR",
          Ci.nsIWindowsRegKey.WOW64_64
        );
        this._osData.windowsUBR = ubr !== undefined ? ubr : null;
      }
    }

    return this._osData;
  },

  _hddData: null,
  /**
   * Get the HDD information.
   * @return Object containing the HDD data.
   */
  _getHDDData() {
    if (this._hddData) {
      return this._hddData;
    }
    let nullData = { model: null, revision: null, type: null };
    return { profile: nullData, binary: nullData, system: nullData };
  },

  /**
   * Get registered security product information.
   * @return Object containing the security product data
   */
  _getSecurityAppData() {
    const maxStringLength = 256;

    const keys = [
      ["registeredAntiVirus", "antivirus"],
      ["registeredAntiSpyware", "antispyware"],
      ["registeredFirewall", "firewall"],
    ];

    let result = {};

    for (let [inKey, outKey] of keys) {
      let prop = getSysinfoProperty(inKey, null);
      if (prop) {
        prop = limitStringToLength(prop, maxStringLength).split(";");
      }

      result[outKey] = prop;
    }

    return result;
  },

  /**
   * Get the GFX information.
   * @return Object containing the GFX data.
   */
  _getGFXData() {
    let gfxData = {
      D2DEnabled: getGfxField("D2DEnabled", null),
      DWriteEnabled: getGfxField("DWriteEnabled", null),
      ContentBackend: getGfxField("ContentBackend", null),
      Headless: getGfxField("isHeadless", null),
      EmbeddedInFirefoxReality: getGfxField("EmbeddedInFirefoxReality", null),
      // The following line is disabled due to main thread jank and will be enabled
      // again as part of bug 1154500.
      // DWriteVersion: getGfxField("DWriteVersion", null),
      adapters: [],
      monitors: [],
      features: {},
    };

    if (AppConstants.platform !== "android") {
      let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
      try {
        gfxData.monitors = gfxInfo.getMonitors();
      } catch (e) {
        this._log.error("nsIGfxInfo.getMonitors() caught error", e);
      }
    }

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
    gfxData.adapters[1].GPUActive = getGfxField("isGPU2Active", null);

    return gfxData;
  },

  /**
   * Get the system data in object form.
   * @return Object containing the system data.
   */
  _getSystem() {
    let memoryMB = getSysinfoProperty("memsize", null);
    if (memoryMB) {
      // Send RAM size in megabytes. Rounding because sysinfo doesn't
      // always provide RAM in multiples of 1024.
      memoryMB = Math.round(memoryMB / 1024 / 1024);
    }

    let virtualMB = getSysinfoProperty("virtualmemsize", null);
    if (virtualMB) {
      // Send the total virtual memory size in megabytes. Rounding because
      // sysinfo doesn't always provide RAM in multiples of 1024.
      virtualMB = Math.round(virtualMB / 1024 / 1024);
    }

    let data = {
      memoryMB,
      virtualMaxMB: virtualMB,
      cpu: this._getCPUData(),
      os: this._getOSData(),
      hdd: this._getHDDData(),
      gfx: this._getGFXData(),
      appleModelId: getSysinfoProperty("appleModelId", null),
      hasWinPackageId: getSysinfoProperty("hasWinPackageId", null),
    };

    if (AppConstants.platform === "win") {
      // This is only sent for Mozilla produced MSIX packages
      let winPackageFamilyName = getSysinfoProperty("winPackageFamilyName", "");
      if (
        winPackageFamilyName.startsWith("Mozilla.") ||
        winPackageFamilyName.startsWith("MozillaCorporation.")
      ) {
        data = { winPackageFamilyName };
      }
      data = { ...this._getProcessData(), ...data };
    } else if (AppConstants.platform == "android") {
      data.device = this._getDeviceData();
    }

    // Windows 8+
    if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
      data.sec = this._getSecurityAppData();
    }

    return data;
  },

  _onEnvironmentChange(what, oldEnvironment) {
    this._log.trace("_onEnvironmentChange for " + what);

    // We are already skipping change events in _checkChanges if there is a pending change task running.
    if (this._shutdown) {
      this._log.trace("_onEnvironmentChange - Already shut down.");
      return;
    }

    if (ObjectUtils.deepEqual(this._currentEnvironment, oldEnvironment)) {
      this._log.trace("_onEnvironmentChange - Environment didn't change");
      return;
    }

    for (let [name, listener] of this._changeListeners) {
      try {
        this._log.debug("_onEnvironmentChange - calling " + name);
        listener(what, oldEnvironment);
      } catch (e) {
        this._log.error(
          "_onEnvironmentChange - listener " + name + " caught error",
          e
        );
      }
    }
  },

  reset() {
    this._shutdown = false;
  },
};
