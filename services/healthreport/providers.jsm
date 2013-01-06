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
  "AppInfoProvider",
  "SysInfoProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");


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
    });
  },
});


this.AppInfoProvider = function AppInfoProvider() {
  Metrics.Provider.call(this);

  this._prefs = new Preferences({defaultBranch: null});
}
AppInfoProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.appInfo",

  measurementTypes: [AppInfoMeasurement],

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

