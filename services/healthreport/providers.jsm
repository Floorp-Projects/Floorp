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

this.EXPORTED_SYMBOLS = [
  "AddonsProvider",
  "AppInfoProvider",
  "CrashDirectoryService",
  "CrashesProvider",
  "SessionsProvider",
  "SysInfoProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

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
  version: 1,

  LAST_TEXT_FIELDS: [
    "vendor",
    "name",
    "id",
    "version",
    "appBuildID",
    "platformVersion",
    "platformBuildID",
    "os",
    "xpcomabi",
    "updateChannel",
    "distributionID",
    "distributionVersion",
    "hotfixVersion",
    "locale",
  ],

  configureStorage: function () {
    let self = this;
    return Task.spawn(function configureStorage() {
      for (let field of self.LAST_TEXT_FIELDS) {
        yield self.registerStorageField(field, self.storage.FIELD_LAST_TEXT);
      }

      yield self.registerStorageField("isDefaultBrowser",
                                      self.storage.FIELD_DAILY_LAST_NUMERIC);
    });
  },
});


function AppVersionMeasurement() {
  Metrics.Measurement.call(this);
}

AppVersionMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "versions",
  version: 1,

  configureStorage: function () {
    return this.registerStorageField("version",
                                     this.storage.FIELD_DAILY_DISCRETE_TEXT);
  },
});



this.AppInfoProvider = function AppInfoProvider() {
  Metrics.Provider.call(this);

  this._prefs = new Preferences({defaultBranch: null});
}
AppInfoProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.appInfo",

  measurementTypes: [AppInfoMeasurement, AppVersionMeasurement],

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

  onInit: function () {
    return Task.spawn(this._onInit.bind(this));
  },

  _onInit: function () {
    // Services.appInfo should always be defined for any reasonably behaving
    // Gecko app. If it isn't, we insert a empty string sentinel value.
    let ai;
    try {
      ai = Services.appinfo;
    } catch (ex) {
      this._log.error("Could not obtain Services.appinfo: " +
                     CommonUtils.exceptionStr(ex));
      yield this._setCurrentVersion("");
      return;
    }

    if (!ai) {
      this._log.error("Services.appinfo is unavailable.");
      yield this._setCurrentVersion("");
      return;
    }

    let currentVersion = ai.version;
    let lastVersion = yield this.getState("lastVersion");

    if (currentVersion == lastVersion) {
      return;
    }

    yield this._setCurrentVersion(currentVersion);
  },

  _setCurrentVersion: function (version) {
    this._log.info("Recording new application version: " + version);
    let m = this.getMeasurement("versions", 1);
    m.addDailyDiscreteText("version", version);
    return this.setState("lastVersion", version);
  },

  collectConstantData: function () {
    return this.enqueueStorageOperation(function collect() {
      return Task.spawn(this._populateConstants.bind(this));
    }.bind(this));
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
    yield this._recordDefaultBrowser(m);
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

  configureStorage: function () {
    return Task.spawn(function configureStorage() {
      yield this.registerStorageField("cpuCount", this.storage.FIELD_LAST_NUMERIC);
      yield this.registerStorageField("memoryMB", this.storage.FIELD_LAST_NUMERIC);
      yield this.registerStorageField("manufacturer", this.storage.FIELD_LAST_TEXT);
      yield this.registerStorageField("device", this.storage.FIELD_LAST_TEXT);
      yield this.registerStorageField("hardware", this.storage.FIELD_LAST_TEXT);
      yield this.registerStorageField("name", this.storage.FIELD_LAST_TEXT);
      yield this.registerStorageField("version", this.storage.FIELD_LAST_TEXT);
      yield this.registerStorageField("architecture", this.storage.FIELD_LAST_TEXT);
    }.bind(this));
  },
});


this.SysInfoProvider = function SysInfoProvider() {
  Metrics.Provider.call(this);
};

SysInfoProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.sysinfo",

  measurementTypes: [SysInfoMeasurement],

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
    return this.enqueueStorageOperation(function collection() {
      return Task.spawn(this._populateConstants.bind(this));
    }.bind(this));
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
 */
function CurrentSessionMeasurement() {
  Metrics.Measurement.call(this);
}

CurrentSessionMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "current",
  version: 1,

  LAST_NUMERIC_FIELDS: [
    // Day on which the session was started.
    // This is used to determine which day the record will be moved under when
    // moved to daily sessions.
    "startDay",

    // Time in milliseconds the session was active for.
    "activeTime",

    // Total time in milliseconds of the session.
    "totalTime",

    // Startup times, in milliseconds.
    "main",
    "firstPaint",
    "sessionRestored",
  ],

  configureStorage: function () {
    return Task.spawn(function configureStorage() {
      for (let field of this.LAST_NUMERIC_FIELDS) {
        yield this.registerStorageField(field, this.storage.FIELD_LAST_NUMERIC);
      }
    }.bind(this));
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
  version: 1,

  DAILY_DISCRETE_NUMERIC_FIELDS: [
    // Milliseconds of sessions that were properly shut down.
    "cleanActiveTime",
    "cleanTotalTime",

    // Milliseconds of sessions that were not properly shut down.
    "abortedActiveTime",
    "abortedTotalTime",

    // Startup times in milliseconds.
    "main",
    "firstPaint",
    "sessionRestored",
  ],

  configureStorage: function () {
    return Task.spawn(function configureStorage() {
      for (let field of this.DAILY_DISCRETE_NUMERIC_FIELDS) {
        yield this.registerStorageField(field, this.storage.FIELD_DAILY_DISCRETE_NUMERIC);
      }
    }.bind(this));
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

  this._startDate = null;
  this._currentActiveTime = null;
  this._lastActivityDate = null;
  this._lastActivityWasInactive = false;
};

SessionsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.appSessions",

  measurementTypes: [CurrentSessionMeasurement, PreviousSessionsMeasurement],

  _OBSERVERS: ["user-interaction-active", "user-interaction-inactive"],

  onInit: function () {
    return Task.spawn(this._onInit.bind(this));
  },

  _onInit: function () {
    // We could cross day boundary between the application started and when
    // this code is called. Meh.
    let now = new Date();
    this._startDate = now;
    let current = this.getMeasurement("current", 1);

    // Initialization occurs serially so we don't need to enqueue
    // storage operations.
    let currentData = yield this.storage.getMeasurementLastValuesFromMeasurementID(current.id);

    // Normal shutdown should purge all data for this measurement. If
    // there is data here, the session was aborted.
    if (currentData.size) {
      this._log.info("Data left over from old session. Counting as aborted.");
      yield Task.spawn(this._moveCurrentToDaily.bind(this, currentData, true));
    }

    this._currentActiveTime = 0;
    this._lastActivityDate = now;

    this._log.debug("Registering new/current session.");
    yield current.setLastNumeric("activeTime", 0, now);
    yield current.setLastNumeric("totalTime", 0, now);
    yield current.setLastNumeric("startDay", this._dateToDays(now), now);

    let si = this._getStartupInfo();

    for (let field of ["main", "firstPaint", "sessionRestored"]) {
      if (!(field in si)) {
        continue;
      }

      // si.process is the Date when the process actually started.
      let value = si[field] - si.process;
      yield current.setLastNumeric(field, value, now);
    }

    for (let channel of this._OBSERVERS) {
      Services.obs.addObserver(this, channel, false);
    }
  },

  onShutdown: function () {
    for (let channel of this._OBSERVERS) {
      Services.obs.removeObserver(this, channel);
    }

    return Task.spawn(this._onShutdown.bind(this));
  },

  _onShutdown: function () {
    this._log.debug("Recording clean shutdown.");
    yield this.recordBrowserActivity(true);
    let current = this.getMeasurement("current", 1);

    let self = this;
    yield this.enqueueStorageOperation(function doShutdown() {
      return Task.spawn(function shutdownTask() {
        let data = yield self.storage.getMeasurementLastValuesFromMeasurementID(current.id);
        yield self._moveCurrentToDaily(data, false);
      });
    });
  },

  /**
   * Record browser activity.
   *
   * This should be called periodically to update the stored times of how often
   * the user was active with the browser.
   *
   * The underlying browser activity observer fires every 5 seconds if there
   * is activity. If there is inactivity, it fires after 5 seconds of inactivity
   * and doesn't fire again until there is activity.
   *
   * @param active
   *        (bool) Whether the browser was active or inactive.
   */
  recordBrowserActivity: function (active) {
    // There is potential for clock skew to result in incorrect measurements
    // here. We should count ticks instead of calculating intervals.
    // FUTURE count ticks not intervals.
    let now = new Date();
    this._log.trace("Recording browser activity. Active? " + !!active);

    let m = this.getMeasurement("current", 1);

    let updateActive = active && !this._lastActivityWasInactive;
    this._lastActivityWasInactive = !active;

    if (updateActive) {
      this._currentActiveTime += now - this._lastActivityDate;
    }

    this._lastActivityDate = now;

    let totalTime = now - this._startDate;
    let activeTime = this._currentActiveTime;

    return this.enqueueStorageOperation(function op() {
      let promise = m.setLastNumeric("totalTime", totalTime, now);

      if (!updateActive) {
        return promise;
      }

      return m.setLastNumeric("activeTime", activeTime, now);
    });
  },

  _moveCurrentToDaily: function (fields, aborted) {
    this._log.debug("Moving current session to past. Aborted? " + aborted);
    let current = this.getMeasurement("current", 1);

    function clearCurrent() {
      current.deleteLastNumeric("startDay");
      current.deleteLastNumeric("activeTime");
      current.deleteLastNumeric("totalTime");
      current.deleteLastNumeric("main");
      current.deleteLastNumeric("firstPaint");
      return current.deleteLastNumeric("sessionRestored");
    }

    // We should never have incomplete values. But if we do, handle it
    // gracefully.
    if (!fields.has("startDay") || !fields.has("activeTime") || !fields.has("totalTime")) {
      yield clearCurrent();
      return;
    }

    let daily = this.getMeasurement("previous", 1);

    let startDays = fields.get("startDay")[1];
    let activeTime = fields.get("activeTime")[1];
    let totalTime = fields.get("totalTime")[1];

    let date = this._daysToDate(startDays);
    let type = aborted ? "aborted" : "clean";

    yield daily.addDailyDiscreteNumeric(type + "ActiveTime", activeTime, date);
    yield daily.addDailyDiscreteNumeric(type + "TotalTime", totalTime, date);

    for (let field of ["main", "firstPaint", "sessionRestored"]) {
      if (!fields.has(field)) {
        this._log.info(field + " field not recorded for current session.");
        continue;
      }

      yield daily.addDailyDiscreteNumeric(field, fields.get(field)[1], date);
    }

    yield clearCurrent();
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "user-interaction-active":
        this.recordBrowserActivity(true);
        break;

      case "user-interaction-inactive":
        this.recordBrowserActivity(false);
        break;
    }
  },

  // Implemented as a function to allow for monkeypatching in tests.
  _getStartupInfo: function () {
    return Cc["@mozilla.org/toolkit/app-startup;1"]
             .getService(Ci.nsIAppStartup)
             .getStartupInfo();
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

  configureStorage: function () {
    return this.registerStorageField("addons", this.storage.FIELD_LAST_TEXT);
  },

  _serializeJSONSingular: function (data) {
    if (!data.has("addons")) {
      this._log.warn("Don't have active addons info. Weird.");
      return null;
    }

    // Exceptions are caught in the caller.
    return JSON.parse(data.get("addons")[1]);
  },
});


function AddonCountsMeasurement() {
  Metrics.Measurement.call(this);
}

AddonCountsMeasurement.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "counts",
  version: 1,

  configureStorage: function () {
    return Task.spawn(function registerFields() {
      yield this.registerStorageField("theme", this.storage.FIELD_DAILY_LAST_NUMERIC);
      yield this.registerStorageField("lwtheme", this.storage.FIELD_DAILY_LAST_NUMERIC);
      yield this.registerStorageField("plugin", this.storage.FIELD_DAILY_LAST_NUMERIC);
      yield this.registerStorageField("extension", this.storage.FIELD_DAILY_LAST_NUMERIC);
    }.bind(this));
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

  onInit: function () {
    let listener = {};

    for (let method of this.ADDON_LISTENER_CALLBACKS) {
      listener[method] = this._collectAndStoreAddons.bind(this);
    }

    this._listener = listener;
    AddonManager.addAddonListener(this._listener);

    return Promise.resolve();
  },

  onShutdown: function () {
    AddonManager.removeAddonListener(this._listener);
    this._listener = null;

    return Promise.resolve();
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

  configureStorage: function () {
    this.registerStorageField("pending", this.storage.FIELD_DAILY_COUNTER);
    this.registerStorageField("submitted", this.storage.FIELD_DAILY_COUNTER);
  },
});

this.CrashesProvider = function () {
  Metrics.Provider.call(this);
};

CrashesProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.crashes",

  measurementTypes: [DailyCrashesMeasurement],

  collectConstantData: function () {
    return Task.spawn(this._populateCrashCounts.bind(this));
  },

  _populateCrashCounts: function () {
    let now = new Date();
    let service = new CrashDirectoryService();

    let pending = yield service.getPendingFiles();
    let submitted = yield service.getSubmittedFiles();

    let lastCheck = yield this.getState("lastCheck");
    if (!lastCheck) {
      lastCheck = 0;
    } else {
      lastCheck = parseInt(lastCheck, 10);
      if (Number.isNaN(lastCheck)) {
        lastCheck = 0;
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

