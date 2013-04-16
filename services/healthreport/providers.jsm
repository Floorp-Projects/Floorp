/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains metrics data providers for the Firefox Health
 * Report. Ideally each provider in this file exists in separate modules
 * and lives close to the code it is querying. However, because of the
 * overhead of JS compartments (which are created for each module), we
 * currently have all the code in one file. When the overhead of
 * compartments reaches a reasonable level, this file should be split
 * up.
 */

"use strict";

#ifndef MERGED_COMPARTMENT

this.EXPORTED_SYMBOLS = [
  "AddonsProvider",
  "AppInfoProvider",
  "CrashDirectoryService",
  "CrashesProvider",
  "PlacesProvider",
  "SearchesProvider",
  "SessionsProvider",
  "SysInfoProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");

#endif

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesDBUtils",
                                  "resource://gre/modules/PlacesDBUtils.jsm");


const LAST_TEXT_FIELD = {type: Metrics.Storage.FIELD_LAST_TEXT};
const DAILY_DISCRETE_NUMERIC_FIELD = {type: Metrics.Storage.FIELD_DAILY_DISCRETE_NUMERIC};
const DAILY_LAST_NUMERIC_FIELD = {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC};
const DAILY_COUNTER_FIELD = {type: Metrics.Storage.FIELD_DAILY_COUNTER};

// Preprocess to use the correct telemetry pref.
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
const TELEMETRY_PREF = "toolkit.telemetry.enabledPreRelease";
#else
const TELEMETRY_PREF = "toolkit.telemetry.enabled";
#endif

/**
 * Represents basic application state.
 *
 * This is roughly a union of nsIXULAppInfo, nsIXULRuntime, with a few extra
 * pieces thrown in.
 */
function AppInfoMeasurement() {
  Metrics.Measurement.call(this);
}

AppInfoMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "appinfo",
  version: 2,

  fields: {
    vendor: LAST_TEXT_FIELD,
    name: LAST_TEXT_FIELD,
    id: LAST_TEXT_FIELD,
    version: LAST_TEXT_FIELD,
    appBuildID: LAST_TEXT_FIELD,
    platformVersion: LAST_TEXT_FIELD,
    platformBuildID: LAST_TEXT_FIELD,
    os: LAST_TEXT_FIELD,
    xpcomabi: LAST_TEXT_FIELD,
    updateChannel: LAST_TEXT_FIELD,
    distributionID: LAST_TEXT_FIELD,
    distributionVersion: LAST_TEXT_FIELD,
    hotfixVersion: LAST_TEXT_FIELD,
    locale: LAST_TEXT_FIELD,
    isDefaultBrowser: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    isTelemetryEnabled: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    isBlocklistEnabled: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
  },
});

/**
 * Legacy version of app info before Telemetry was added.
 *
 * The "last" fields have all been removed. We only report the longitudinal
 * field.
 */
function AppInfoMeasurement1() {
  Metrics.Measurement.call(this);
}

AppInfoMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "appinfo",
  version: 1,

  fields: {
    isDefaultBrowser: {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
  },
});


function AppVersionMeasurement1() {
  Metrics.Measurement.call(this);
}

AppVersionMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "versions",
  version: 1,

  fields: {
    version: {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
  },
});

// Version 2 added the build ID.
function AppVersionMeasurement2() {
  Metrics.Measurement.call(this);
}

AppVersionMeasurement2.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "versions",
  version: 2,

  fields: {
    appVersion: {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
    platformVersion: {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
    appBuildID: {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
    platformBuildID: {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
  },
});


this.AppInfoProvider = function AppInfoProvider() {
  Metrics.Provider.call(this);

  this._prefs = new Preferences({defaultBranch: null});
}
AppInfoProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.appInfo",

  measurementTypes: [
    AppInfoMeasurement,
    AppInfoMeasurement1,
    AppVersionMeasurement1,
    AppVersionMeasurement2,
  ],

  pullOnly: true,

  appInfoFields: {
    // From nsIXULAppInfo.
    vendor: "vendor",
    name: "name",
    id: "ID",
    version: "version",
    appBuildID: "appBuildID",
    platformVersion: "platformVersion",
    platformBuildID: "platformBuildID",

    // From nsIXULRuntime.
    os: "OS",
    xpcomabi: "XPCOMABI",
  },

  postInit: function () {
    return Task.spawn(this._postInit.bind(this));
  },

  _postInit: function () {
    let recordEmptyAppInfo = function () {
      this._setCurrentAppVersion("");
      this._setCurrentPlatformVersion("");
      this._setCurrentAppBuildID("");
      return this._setCurrentPlatformBuildID("");
    }.bind(this);

    // Services.appInfo should always be defined for any reasonably behaving
    // Gecko app. If it isn't, we insert a empty string sentinel value.
    let ai;
    try {
      ai = Services.appinfo;
    } catch (ex) {
      this._log.error("Could not obtain Services.appinfo: " +
                     CommonUtils.exceptionStr(ex));
      yield recordEmptyAppInfo();
      return;
    }

    if (!ai) {
      this._log.error("Services.appinfo is unavailable.");
      yield recordEmptyAppInfo();
      return;
    }

    let currentAppVersion = ai.version;
    let currentPlatformVersion = ai.platformVersion;
    let currentAppBuildID = ai.appBuildID;
    let currentPlatformBuildID = ai.platformBuildID;

    // State's name doesn't contain "app" for historical compatibility.
    let lastAppVersion = yield this.getState("lastVersion");
    let lastPlatformVersion = yield this.getState("lastPlatformVersion");
    let lastAppBuildID = yield this.getState("lastAppBuildID");
    let lastPlatformBuildID = yield this.getState("lastPlatformBuildID");

    if (currentAppVersion != lastAppVersion) {
      yield this._setCurrentAppVersion(currentAppVersion);
    }

    if (currentPlatformVersion != lastPlatformVersion) {
      yield this._setCurrentPlatformVersion(currentPlatformVersion);
    }

    if (currentAppBuildID != lastAppBuildID) {
      yield this._setCurrentAppBuildID(currentAppBuildID);
    }

    if (currentPlatformBuildID != lastPlatformBuildID) {
      yield this._setCurrentPlatformBuildID(currentPlatformBuildID);
    }
  },

  _setCurrentAppVersion: function (version) {
    this._log.info("Recording new application version: " + version);
    let m = this.getMeasurement("versions", 2);
    m.addDailyDiscreteText("appVersion", version);

    // "app" not encoded in key for historical compatibility.
    return this.setState("lastVersion", version);
  },

  _setCurrentPlatformVersion: function (version) {
    this._log.info("Recording new platform version: " + version);
    let m = this.getMeasurement("versions", 2);
    m.addDailyDiscreteText("platformVersion", version);
    return this.setState("lastPlatformVersion", version);
  },

  _setCurrentAppBuildID: function (build) {
    this._log.info("Recording new application build ID: " + build);
    let m = this.getMeasurement("versions", 2);
    m.addDailyDiscreteText("appBuildID", build);
    return this.setState("lastAppBuildID", build);
  },

  _setCurrentPlatformBuildID: function (build) {
    this._log.info("Recording new platform build ID: " + build);
    let m = this.getMeasurement("versions", 2);
    m.addDailyDiscreteText("platformBuildID", build);
    return this.setState("lastPlatformBuildID", build);
  },


  collectConstantData: function () {
    return this.storage.enqueueTransaction(this._populateConstants.bind(this));
  },

  _populateConstants: function () {
    let m = this.getMeasurement(AppInfoMeasurement.prototype.name,
                                AppInfoMeasurement.prototype.version);

    let ai;
    try {
      ai = Services.appinfo;
    } catch (ex) {
      this._log.warn("Could not obtain Services.appinfo: " +
                     CommonUtils.exceptionStr(ex));
      throw ex;
    }

    if (!ai) {
      this._log.warn("Services.appinfo is unavailable.");
      throw ex;
    }

    for (let [k, v] in Iterator(this.appInfoFields)) {
      try {
        yield m.setLastText(k, ai[v]);
      } catch (ex) {
        this._log.warn("Error obtaining Services.appinfo." + v);
      }
    }

    try {
      yield m.setLastText("updateChannel", UpdateChannel.get());
    } catch (ex) {
      this._log.warn("Could not obtain update channel: " +
                     CommonUtils.exceptionStr(ex));
    }

    yield m.setLastText("distributionID", this._prefs.get("distribution.id", ""));
    yield m.setLastText("distributionVersion", this._prefs.get("distribution.version", ""));
    yield m.setLastText("hotfixVersion", this._prefs.get("extensions.hotfix.lastVersion", ""));

    try {
      let locale = Cc["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Ci.nsIXULChromeRegistry)
                     .getSelectedLocale("global");
      yield m.setLastText("locale", locale);
    } catch (ex) {
      this._log.warn("Could not obtain application locale: " +
                     CommonUtils.exceptionStr(ex));
    }

    // FUTURE this should be retrieved periodically or at upload time.
    yield this._recordIsTelemetryEnabled(m);
    yield this._recordIsBlocklistEnabled(m);
    yield this._recordDefaultBrowser(m);
  },

  _recordIsTelemetryEnabled: function (m) {
    let enabled = TELEMETRY_PREF && this._prefs.get(TELEMETRY_PREF, false);
    this._log.debug("Recording telemetry enabled (" + TELEMETRY_PREF + "): " + enabled);
    yield m.setDailyLastNumeric("isTelemetryEnabled", enabled ? 1 : 0);
  },

  _recordIsBlocklistEnabled: function (m) {
    let enabled = this._prefs.get("extensions.blocklist.enabled", false);
    this._log.debug("Recording blocklist enabled: " + enabled);
    yield m.setDailyLastNumeric("isBlocklistEnabled", enabled ? 1 : 0);
  },

  _recordDefaultBrowser: function (m) {
    let shellService;
    try {
      shellService = Cc["@mozilla.org/browser/shell-service;1"]
                       .getService(Ci.nsIShellService);
    } catch (ex) {
      this._log.warn("Could not obtain shell service: " +
                     CommonUtils.exceptionStr(ex));
    }

    let isDefault = -1;

    if (shellService) {
      try {
        // This uses the same set of flags used by the pref pane.
        isDefault = shellService.isDefaultBrowser(false, true) ? 1 : 0;
      } catch (ex) {
        this._log.warn("Could not determine if default browser: " +
                       CommonUtils.exceptionStr(ex));
      }
    }

    return m.setDailyLastNumeric("isDefaultBrowser", isDefault);
  },
});


function SysInfoMeasurement() {
  Metrics.Measurement.call(this);
}

SysInfoMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "sysinfo",
  version: 1,

  fields: {
    cpuCount: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
    memoryMB: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
    manufacturer: LAST_TEXT_FIELD,
    device: LAST_TEXT_FIELD,
    hardware: LAST_TEXT_FIELD,
    name: LAST_TEXT_FIELD,
    version: LAST_TEXT_FIELD,
    architecture: LAST_TEXT_FIELD,
  },
});


this.SysInfoProvider = function SysInfoProvider() {
  Metrics.Provider.call(this);
};

SysInfoProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.sysinfo",

  measurementTypes: [SysInfoMeasurement],

  pullOnly: true,

  sysInfoFields: {
    cpucount: "cpuCount",
    memsize: "memoryMB",
    manufacturer: "manufacturer",
    device: "device",
    hardware: "hardware",
    name: "name",
    version: "version",
    arch: "architecture",
  },

  collectConstantData: function () {
    return this.storage.enqueueTransaction(this._populateConstants.bind(this));
  },

  _populateConstants: function () {
    let m = this.getMeasurement(SysInfoMeasurement.prototype.name,
                                SysInfoMeasurement.prototype.version);

    let si = Cc["@mozilla.org/system-info;1"]
               .getService(Ci.nsIPropertyBag2);

    for (let [k, v] in Iterator(this.sysInfoFields)) {
      try {
        if (!si.hasKey(k)) {
          this._log.debug("Property not available: " + k);
          continue;
        }

        let value = si.getProperty(k);
        let method = "setLastText";

        if (["cpucount", "memsize"].indexOf(k) != -1) {
          let converted = parseInt(value, 10);
          if (Number.isNaN(converted)) {
            continue;
          }

          value = converted;
          method = "setLastNumeric";
        }

        // Round memory to mebibytes.
        if (k == "memsize") {
          value = Math.round(value / 1048576);
        }

        yield m[method](v, value);
      } catch (ex) {
        this._log.warn("Error obtaining system info field: " + k + " " +
                       CommonUtils.exceptionStr(ex));
      }
    }
  },
});


/**
 * Holds information about the current/active session.
 *
 * The fields within the current session are moved to daily session fields when
 * the application is shut down.
 *
 * This measurement is backed by the SessionRecorder, not the database.
 */
function CurrentSessionMeasurement() {
  Metrics.Measurement.call(this);
}

CurrentSessionMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "current",
  version: 3,

  // Storage is in preferences.
  fields: {},

  /**
   * All data is stored in prefs, so we have a custom implementation.
   */
  getValues: function () {
    let sessions = this.provider.healthReporter.sessionRecorder;

    let fields = new Map();
    let now = new Date();
    fields.set("startDay", [now, Metrics.dateToDays(sessions.startDate)]);
    fields.set("activeTicks", [now, sessions.activeTicks]);
    fields.set("totalTime", [now, sessions.totalTime]);
    fields.set("main", [now, sessions.main]);
    fields.set("firstPaint", [now, sessions.firstPaint]);
    fields.set("sessionRestored", [now, sessions.sessionRestored]);

    return CommonUtils.laterTickResolvingPromise({
      days: new Metrics.DailyValues(),
      singular: fields,
    });
  },

  _serializeJSONSingular: function (data) {
    let result = {"_v": this.version};

    for (let [field, value] of data) {
      result[field] = value[1];
    }

    return result;
  },
});

/**
 * Records a history of all application sessions.
 */
function PreviousSessionsMeasurement() {
  Metrics.Measurement.call(this);
}

PreviousSessionsMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "previous",
  version: 3,

  fields: {
    // Milliseconds of sessions that were properly shut down.
    cleanActiveTicks: DAILY_DISCRETE_NUMERIC_FIELD,
    cleanTotalTime: DAILY_DISCRETE_NUMERIC_FIELD,

    // Milliseconds of sessions that were not properly shut down.
    abortedActiveTicks: DAILY_DISCRETE_NUMERIC_FIELD,
    abortedTotalTime: DAILY_DISCRETE_NUMERIC_FIELD,

    // Startup times in milliseconds.
    main: DAILY_DISCRETE_NUMERIC_FIELD,
    firstPaint: DAILY_DISCRETE_NUMERIC_FIELD,
    sessionRestored: DAILY_DISCRETE_NUMERIC_FIELD,
  },
});


/**
 * Records information about the current browser session.
 *
 * A browser session is defined as an application/process lifetime. We
 * start a new session when the application starts (essentially when
 * this provider is instantiated) and end the session on shutdown.
 *
 * As the application runs, we record basic information about the
 * "activity" of the session. Activity is defined by the presence of
 * physical input into the browser (key press, mouse click, touch, etc).
 *
 * We differentiate between regular sessions and "aborted" sessions. An
 * aborted session is one that does not end expectedly. This is often the
 * result of a crash. We detect aborted sessions by storing the current
 * session separate from completed sessions. We normally move the
 * current session to completed sessions on application shutdown. If a
 * current session is present on application startup, that means that
 * the previous session was aborted.
 */
this.SessionsProvider = function () {
  Metrics.Provider.call(this);
};

SessionsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.appSessions",

  measurementTypes: [CurrentSessionMeasurement, PreviousSessionsMeasurement],

  pullOnly: true,

  collectConstantData: function () {
    let previous = this.getMeasurement("previous", 3);

    return this.storage.enqueueTransaction(this._recordAndPruneSessions.bind(this));
  },

  _recordAndPruneSessions: function () {
    this._log.info("Moving previous sessions from session recorder to storage.");
    let recorder = this.healthReporter.sessionRecorder;
    let sessions = recorder.getPreviousSessions();
    this._log.debug("Found " + Object.keys(sessions).length + " previous sessions.");

    let daily = this.getMeasurement("previous", 3);

    // Please note the coupling here between the session recorder and our state.
    // If the pruned index or the current index of the session recorder is ever
    // deleted or reset to 0, our stored state of a later index would mean that
    // new sessions would never be captured by this provider until the session
    // recorder index catches up to our last session ID. This should not happen
    // under normal circumstances, so we don't worry too much about it. We
    // should, however, consider this as part of implementing bug 841561.
    let lastRecordedSession = yield this.getState("lastSession");
    if (lastRecordedSession === null) {
      lastRecordedSession = -1;
    }
    this._log.debug("The last recorded session was #" + lastRecordedSession);

    for (let [index, session] in Iterator(sessions)) {
      if (index < lastRecordedSession) {
        this._log.warn("Already recorded session " + index + ". Did the last " +
                       "session crash or have an issue saving the prefs file?");
        continue;
      }

      let type = session.clean ? "clean" : "aborted";
      let date = session.startDate;
      yield daily.addDailyDiscreteNumeric(type + "ActiveTicks", session.activeTicks, date);
      yield daily.addDailyDiscreteNumeric(type + "TotalTime", session.totalTime, date);

      for (let field of ["main", "firstPaint", "sessionRestored"]) {
        yield daily.addDailyDiscreteNumeric(field, session[field], date);
      }

      lastRecordedSession = index;
    }

    yield this.setState("lastSession", "" + lastRecordedSession);
    recorder.pruneOldSessions(new Date());
  },
});

/**
 * Stores the set of active addons in storage.
 *
 * We do things a little differently than most other measurements. Because
 * addons are difficult to shoehorn into distinct fields, we simply store a
 * JSON blob in storage in a text field.
 */
function ActiveAddonsMeasurement() {
  Metrics.Measurement.call(this);

  this._serializers = {};
  this._serializers[this.SERIALIZE_JSON] = {
    singular: this._serializeJSONSingular.bind(this),
    // We don't need a daily serializer because we have none of this data.
  };
}

ActiveAddonsMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "active",
  version: 1,

  fields: {
    addons: LAST_TEXT_FIELD,
  },

  _serializeJSONSingular: function (data) {
    if (!data.has("addons")) {
      this._log.warn("Don't have active addons info. Weird.");
      return null;
    }

    // Exceptions are caught in the caller.
    let result = JSON.parse(data.get("addons")[1]);
    result._v = this.version;
    return result;
  },
});


function AddonCountsMeasurement() {
  Metrics.Measurement.call(this);
}

AddonCountsMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "counts",
  version: 1,

  fields: {
    theme: DAILY_LAST_NUMERIC_FIELD,
    lwtheme: DAILY_LAST_NUMERIC_FIELD,
    plugin: DAILY_LAST_NUMERIC_FIELD,
    extension: DAILY_LAST_NUMERIC_FIELD,
  },
});


this.AddonsProvider = function () {
  Metrics.Provider.call(this);

  this._prefs = new Preferences({defaultBranch: null});
};

AddonsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  // Whenever these AddonListener callbacks are called, we repopulate
  // and store the set of addons. Note that these events will only fire
  // for restartless add-ons. For actions that require a restart, we
  // will catch the change after restart. The alternative is a lot of
  // state tracking here, which isn't desirable.
  ADDON_LISTENER_CALLBACKS: [
    "onEnabled",
    "onDisabled",
    "onInstalled",
    "onUninstalled",
  ],

  // Add-on types for which full details are uploaded in the
  // ActiveAddonsMeasurement. All other types are ignored.
  FULL_DETAIL_TYPES: [
    "plugin",
    "extension",
  ],

  name: "org.mozilla.addons",

  measurementTypes: [
    ActiveAddonsMeasurement,
    AddonCountsMeasurement,
  ],

  postInit: function () {
    let listener = {};

    for (let method of this.ADDON_LISTENER_CALLBACKS) {
      listener[method] = this._collectAndStoreAddons.bind(this);
    }

    this._listener = listener;
    AddonManager.addAddonListener(this._listener);

    return CommonUtils.laterTickResolvingPromise();
  },

  onShutdown: function () {
    AddonManager.removeAddonListener(this._listener);
    this._listener = null;

    return CommonUtils.laterTickResolvingPromise();
  },

  collectConstantData: function () {
    return this._collectAndStoreAddons();
  },

  _collectAndStoreAddons: function () {
    let deferred = Promise.defer();

    AddonManager.getAllAddons(function onAllAddons(addons) {
      let data;
      let addonsField;
      try {
        data = this._createDataStructure(addons);
        addonsField = JSON.stringify(data.addons);
      } catch (ex) {
        this._log.warn("Exception when populating add-ons data structure: " +
                       CommonUtils.exceptionStr(ex));
        deferred.reject(ex);
        return;
      }

      let now = new Date();
      let active = this.getMeasurement("active", 1);
      let counts = this.getMeasurement("counts", 1);

      this.enqueueStorageOperation(function storageAddons() {
        for (let type in data.counts) {
          try {
            counts.fieldID(type);
          } catch (ex) {
            this._log.warn("Add-on type without field: " + type);
            continue;
          }

          counts.setDailyLastNumeric(type, data.counts[type], now);
        }

        return active.setLastText("addons", addonsField).then(
          function onSuccess() { deferred.resolve(); },
          function onError(error) { deferred.reject(error); }
        );
      }.bind(this));
    }.bind(this));

    return deferred.promise;
  },

  COPY_FIELDS: [
    "userDisabled",
    "appDisabled",
    "version",
    "type",
    "scope",
    "foreignInstall",
    "hasBinaryComponents",
  ],

  _createDataStructure: function (addons) {
    let data = {addons: {}, counts: {}};

    for (let addon of addons) {
      let type = addon.type;
      data.counts[type] = (data.counts[type] || 0) + 1;

      if (this.FULL_DETAIL_TYPES.indexOf(addon.type) == -1) {
        continue;
      }

      let optOutPref = "extensions." + addon.id + ".getAddons.cache.enabled";
      if (!this._prefs.get(optOutPref, true)) {
        this._log.debug("Ignoring add-on that's opted out of AMO updates: " +
                        addon.id);
        continue;
      }

      let obj = {};
      for (let field of this.COPY_FIELDS) {
        obj[field] = addon[field];
      }

      if (addon.installDate) {
        obj.installDay = this._dateToDays(addon.installDate);
      }

      if (addon.updateDate) {
        obj.updateDay = this._dateToDays(addon.updateDate);
      }

      data.addons[addon.id] = obj;

    }

    return data;
  },
});


function DailyCrashesMeasurement() {
  Metrics.Measurement.call(this);
}

DailyCrashesMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "crashes",
  version: 1,

  fields: {
    pending: DAILY_COUNTER_FIELD,
    submitted: DAILY_COUNTER_FIELD,
  },
});

this.CrashesProvider = function () {
  Metrics.Provider.call(this);
};

CrashesProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.crashes",

  measurementTypes: [DailyCrashesMeasurement],

  pullOnly: true,

  collectConstantData: function () {
    return this.storage.enqueueTransaction(this._populateCrashCounts.bind(this));
  },

  _populateCrashCounts: function () {
    let now = new Date();
    let service = new CrashDirectoryService();

    let pending = yield service.getPendingFiles();
    let submitted = yield service.getSubmittedFiles();

    function getAgeLimit() {
      return 0;
    }

    let lastCheck = yield this.getState("lastCheck");
    if (!lastCheck) {
      lastCheck = getAgeLimit();
    } else {
      lastCheck = parseInt(lastCheck, 10);
      if (Number.isNaN(lastCheck)) {
        lastCheck = getAgeLimit();
      }
    }

    let m = this.getMeasurement("crashes", 1);

    // FUTURE detect mtimes in the future and react more intelligently.
    for (let filename in pending) {
      let modified = pending[filename].modified;

      if (modified.getTime() < lastCheck) {
        continue;
      }

      yield m.incrementDailyCounter("pending", modified);
    }

    for (let filename in submitted) {
      let modified = submitted[filename].modified;

      if (modified.getTime() < lastCheck) {
        continue;
      }

      yield m.incrementDailyCounter("submitted", modified);
    }

    yield this.setState("lastCheck", "" + now.getTime());
  },
});


/**
 * Helper for interacting with the crashes directory.
 *
 * FUTURE Extract to JSM alongside crashreporter. Use in about:crashes.
 */
this.CrashDirectoryService = function () {
  let base = Cc["@mozilla.org/file/directory_service;1"]
               .getService(Ci.nsIProperties)
               .get("UAppData", Ci.nsIFile);

  let cr = base.clone();
  cr.append("Crash Reports");

  let submitted = cr.clone();
  submitted.append("submitted");

  let pending = cr.clone();
  pending.append("pending");

  this._baseDir = base.path;
  this._submittedDir = submitted.path;
  this._pendingDir = pending.path;
};

CrashDirectoryService.prototype = Object.freeze({
  RE_SUBMITTED_FILENAME: /^bp-.+\.txt$/,
  RE_PENDING_FILENAME: /^.+\.dmp$/,

  getPendingFiles: function () {
    return this._getDirectoryEntries(this._pendingDir,
                                     this.RE_PENDING_FILENAME);
  },

  getSubmittedFiles: function () {
    return this._getDirectoryEntries(this._submittedDir,
                                     this.RE_SUBMITTED_FILENAME);
  },

  _getDirectoryEntries: function (path, re) {
    let files = {};

    return Task.spawn(function iterateDirectory() {
      // If the directory doesn't exist, exit immediately. Else, re-throw
      // any errors.
      try {
        yield OS.File.stat(path);
      } catch (ex if ex instanceof OS.File.Error) {
        if (ex.becauseNoSuchFile) {
          throw new Task.Result({});
        }

        throw ex;
      }

      let iterator = new OS.File.DirectoryIterator(path);

      try {
        while (true) {
          let entry;
          try {
            entry = yield iterator.next();
          } catch (ex if ex == StopIteration) {
            break;
          }

          if (!entry.name.match(re)) {
            continue;
          }

          let info = yield OS.File.stat(entry.path);
          files[entry.name] = {
            created: info.creationDate,
            modified: info.lastModificationDate,
            size: info.size,
          };
        }

        throw new Task.Result(files);
      } finally {
        iterator.close();
      }
    });
  },
});


/**
 * Holds basic statistics about the Places database.
 */
function PlacesMeasurement() {
  Metrics.Measurement.call(this);
}

PlacesMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "places",
  version: 1,

  fields: {
    pages: DAILY_LAST_NUMERIC_FIELD,
    bookmarks: DAILY_LAST_NUMERIC_FIELD,
  },
});


/**
 * Collects information about Places.
 */
this.PlacesProvider = function () {
  Metrics.Provider.call(this);
};

PlacesProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.places",

  measurementTypes: [PlacesMeasurement],

  collectDailyData: function () {
    return this.storage.enqueueTransaction(this._collectData.bind(this));
  },

  _collectData: function () {
    let now = new Date();
    let data = yield this._getDailyValues();

    let m = this.getMeasurement("places", 1);

    yield m.setDailyLastNumeric("pages", data.PLACES_PAGES_COUNT);
    yield m.setDailyLastNumeric("bookmarks", data.PLACES_BOOKMARKS_COUNT);
  },

  _getDailyValues: function () {
    let deferred = Promise.defer();

    PlacesDBUtils.telemetry(null, function onResult(data) {
      deferred.resolve(data);
    });

    return deferred.promise;
  },
});

function SearchCountMeasurement1() {
  Metrics.Measurement.call(this);
}

SearchCountMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "counts",
  version: 1,

  // We only record searches for search engines that have partner agreements
  // with Mozilla.
  fields: {
    "amazon.com.abouthome": DAILY_COUNTER_FIELD,
    "amazon.com.contextmenu": DAILY_COUNTER_FIELD,
    "amazon.com.searchbar": DAILY_COUNTER_FIELD,
    "amazon.com.urlbar": DAILY_COUNTER_FIELD,
    "bing.abouthome": DAILY_COUNTER_FIELD,
    "bing.contextmenu": DAILY_COUNTER_FIELD,
    "bing.searchbar": DAILY_COUNTER_FIELD,
    "bing.urlbar": DAILY_COUNTER_FIELD,
    "google.abouthome": DAILY_COUNTER_FIELD,
    "google.contextmenu": DAILY_COUNTER_FIELD,
    "google.searchbar": DAILY_COUNTER_FIELD,
    "google.urlbar": DAILY_COUNTER_FIELD,
    "yahoo.abouthome": DAILY_COUNTER_FIELD,
    "yahoo.contextmenu": DAILY_COUNTER_FIELD,
    "yahoo.searchbar": DAILY_COUNTER_FIELD,
    "yahoo.urlbar": DAILY_COUNTER_FIELD,
    "other.abouthome": DAILY_COUNTER_FIELD,
    "other.contextmenu": DAILY_COUNTER_FIELD,
    "other.searchbar": DAILY_COUNTER_FIELD,
    "other.urlbar": DAILY_COUNTER_FIELD,
  },
});

/**
 * Records search counts per day per engine and where search initiated.
 *
 * We want to record granular details for individual locale-specific search
 * providers, but only if they're Mozilla partners. In order to do this, we
 * track the nsISearchEngine identifier, which denotes shipped search engines,
 * and intersect those with our partner list.
 *
 * We don't use the search engine name directly, because it is shared across
 * locales; e.g., eBay-de and eBay both share the name "eBay".
 */
function SearchCountMeasurement2() {
  this._fieldSpecs = null;
  this._interestingEngines = null;   // Name -> ID. ("Amazon.com" -> "amazondotcom")

  Metrics.Measurement.call(this);
}

SearchCountMeasurement2.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "counts",
  version: 2,

  /**
   * Default implementation; can be overridden by test helpers.
   */
  getDefaultEngines: function () {
    return Services.search.getDefaultEngines();
  },

  _initialize: function () {
    // Don't create all of these for every profile.
    // There are 61 partner engines, translating to 244 fields.
    // Instead, compute only those that are possible -- those for whom the
    // provider is one of the default search engines.
    // This set can grow over time, and change as users run different localized
    // Firefox instances.
    this._fieldSpecs = {};
    this._interestingEngines = {};

    for (let source of this.SOURCES) {
      this._fieldSpecs["other." + source] = DAILY_COUNTER_FIELD;
    }

    let engines = this.getDefaultEngines();
    for (let engine of engines) {
      let id = engine.identifier;
      if (!id || (this.PROVIDERS.indexOf(id) == -1)) {
        continue;
      }

      this._interestingEngines[engine.name] = id;
      let fieldPrefix = id + ".";
      for (let source of this.SOURCES) {
        this._fieldSpecs[fieldPrefix + source] = DAILY_COUNTER_FIELD;
      }
    }
  },

  // Our fields are dynamic, so we compute them into _fieldSpecs by looking at
  // the current set of interesting engines.
  get fields() {
    if (!this._fieldSpecs) {
      this._initialize();
    }
    return this._fieldSpecs;
  },

  get interestingEngines() {
    if (!this._fieldSpecs) {
      this._initialize();
    }
    return this._interestingEngines;
  },

  /**
   * Override the default behavior: serializers should include every counter
   * field from the DB, even if we don't currently have it registered.
   *
   * Do this so we don't have to register several hundred fields to match
   * various Firefox locales.
   *
   * We use the "provider.type" syntax as a rudimentary check for validity.
   *
   * We trust that measurement versioning is sufficient to exclude old provider
   * data.
   */
  shouldIncludeField: function (name) {
    return name.contains(".");
  },

  /**
   * The measurement type mechanism doesn't introspect the DB. Override it
   * so that we can assume all unknown fields are counters.
   */
  fieldType: function (name) {
    if (name in this.fields) {
      return this.fields[name].type;
    }

    // Default to a counter.
    return Metrics.Storage.FIELD_DAILY_COUNTER;
  },

  // You can compute the total list of fields by unifying the entire l10n repo
  // set with the list of partners:
  //
  //   sort -u */*/searchplugins/list.txt | tr -d '^M' | uniq | grep -f partners.txt
  //
  // where partners.txt contains
  //
  //   amazon
  //   aol
  //   bing
  //   eBay
  //   google
  //   mailru
  //   mercadolibre
  //   seznam
  //   twitter
  //   yahoo
  //   yandex
  //
  // Please update this list as the set of partners changes.
  //
  PROVIDERS: [
    "amazon-co-uk",
    "amazon-de",
    "amazon-en-GB",
    "amazon-france",
    "amazon-it",
    "amazon-jp",
    "amazondotcn",
    "amazondotcom",
    "amazondotcom-de",

    "aol-en-GB",
    "aol-web-search",

    "bing",

    "eBay",
    "eBay-de",
    "eBay-en-GB",
    "eBay-es",
    "eBay-fi",
    "eBay-france",
    "eBay-hu",
    "eBay-in",
    "eBay-it",

    "google",
    "google-jp",
    "google-ku",
    "google-maps-zh-TW",

    "mailru",

    "mercadolibre-ar",
    "mercadolibre-cl",
    "mercadolibre-mx",

    "seznam-cz",

    "twitter",
    "twitter-de",
    "twitter-ja",

    "yahoo",
    "yahoo-NO",
    "yahoo-answer-zh-TW",
    "yahoo-ar",
    "yahoo-bid-zh-TW",
    "yahoo-br",
    "yahoo-ch",
    "yahoo-cl",
    "yahoo-de",
    "yahoo-en-GB",
    "yahoo-es",
    "yahoo-fi",
    "yahoo-france",
    "yahoo-fy-NL",
    "yahoo-id",
    "yahoo-in",
    "yahoo-it",
    "yahoo-jp",
    "yahoo-jp-auctions",
    "yahoo-mx",
    "yahoo-sv-SE",
    "yahoo-zh-TW",

    "yandex",
    "yandex-ru",
    "yandex-slovari",
    "yandex-tr",
    "yandex.by",
    "yandex.ru-be",
  ],

  SOURCES: [
    "abouthome",
    "contextmenu",
    "searchbar",
    "urlbar",
  ],
});

this.SearchesProvider = function () {
  Metrics.Provider.call(this);
};

this.SearchesProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.searches",
  measurementTypes: [
    SearchCountMeasurement1,
    SearchCountMeasurement2,
  ],

  /**
   * Initialize the search service before our measurements are touched.
   */
  preInit: function (storage) {
    // Initialize search service.
    let deferred = Promise.defer();
    Services.search.init(function onInitComplete () {
      deferred.resolve();
    });
    return deferred.promise;
  },

  /**
   * Record that a search occurred.
   *
   * @param engine
   *        (string) The search engine used. If the search engine is unknown,
   *        the search will be attributed to "other".
   * @param source
   *        (string) Where the search was initiated from. Must be one of the
   *        SearchCountMeasurement2.SOURCES values.
   *
   * @return Promise<>
   *         The promise is resolved when the storage operation completes.
   */
  recordSearch: function (engine, source) {
    let m = this.getMeasurement("counts", 2);

    if (m.SOURCES.indexOf(source) == -1) {
      throw new Error("Unknown source for search: " + source);
    }

    let id = m.interestingEngines[engine] || "other";
    let field = id + "." + source;
    return this.enqueueStorageOperation(function recordSearch() {
      return m.incrementDailyCounter(field);
    });
  },
});

