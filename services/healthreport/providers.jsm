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

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/utils.js");


const REQUIRED_STRING_TYPE = {type: "TYPE_STRING"};
const OPTIONAL_STRING_TYPE = {type: "TYPE_STRING", optional: true};
const REQUIRED_UINT32_TYPE = {type: "TYPE_UINT32"};

XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");

/**
 * Represents basic application state.
 *
 * This is roughly a union of nsIXULAppInfo, nsIXULRuntime, with a few extra
 * pieces thrown in.
 */
function AppInfoMeasurement() {
  MetricsMeasurement.call(this, "appinfo", 1);
}

AppInfoMeasurement.prototype = {
  __proto__: MetricsMeasurement.prototype,

  fields: {
    vendor: REQUIRED_STRING_TYPE,
    name: REQUIRED_STRING_TYPE,
    id: REQUIRED_STRING_TYPE,
    version: REQUIRED_STRING_TYPE,
    appBuildID: REQUIRED_STRING_TYPE,
    platformVersion: REQUIRED_STRING_TYPE,
    platformBuildID: REQUIRED_STRING_TYPE,
    os: REQUIRED_STRING_TYPE,
    xpcomabi: REQUIRED_STRING_TYPE,
    updateChannel: REQUIRED_STRING_TYPE,
    distributionID: REQUIRED_STRING_TYPE,
    distributionVersion: REQUIRED_STRING_TYPE,
    hotfixVersion: REQUIRED_STRING_TYPE,
    locale: REQUIRED_STRING_TYPE,
  },
};

Object.freeze(AppInfoMeasurement.prototype);


this.AppInfoProvider = function AppInfoProvider() {
  MetricsProvider.call(this, "app-info");

  this._prefs = new Preferences({defaultBranch: null});
}
AppInfoProvider.prototype = {
  __proto__: MetricsProvider.prototype,

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

  collectConstantMeasurements: function collectConstantMeasurements() {
    let result = this.createResult();
    result.expectMeasurement("appinfo");

    result.populate = this._populateConstants.bind(this);
    return result;
  },

  _populateConstants: function _populateConstants(result) {
    result.addMeasurement(new AppInfoMeasurement());

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
        result.setValue("appinfo", k, ai[v]);
      } catch (ex) {
        this._log.warn("Error obtaining Services.appinfo." + v);
        result.addError(ex);
      }
    }

    try {
      result.setValue("appinfo", "updateChannel", UpdateChannel.get());
    } catch (ex) {
      this._log.warn("Could not obtain update channel: " +
                     CommonUtils.exceptionStr(ex));
      result.addError(ex);
    }

    result.setValue("appinfo", "distributionID", this._prefs.get("distribution.id", ""));
    result.setValue("appinfo", "distributionVersion", this._prefs.get("distribution.version", ""));
    result.setValue("appinfo", "hotfixVersion", this._prefs.get("extensions.hotfix.lastVersion", ""));

    try {
      let locale = Cc["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Ci.nsIXULChromeRegistry)
                     .getSelectedLocale("global");
      result.setValue("appinfo", "locale", locale);
    } catch (ex) {
      this._log.warn("Could not obtain application locale: " +
                     CommonUtils.exceptionStr(ex));
      result.addError(ex);
    }

    result.finish();
  },
};

Object.freeze(AppInfoProvider.prototype);


function SysInfoMeasurement() {
  MetricsMeasurement.call(this, "sysinfo", 1);
}

SysInfoMeasurement.prototype = {
  __proto__: MetricsMeasurement.prototype,

  fields: {
    cpuCount: REQUIRED_UINT32_TYPE,
    memoryMB: REQUIRED_UINT32_TYPE,
    manufacturer: OPTIONAL_STRING_TYPE,
    device: OPTIONAL_STRING_TYPE,
    hardware: OPTIONAL_STRING_TYPE,
    name: OPTIONAL_STRING_TYPE,
    version: OPTIONAL_STRING_TYPE,
    architecture: OPTIONAL_STRING_TYPE,
  },
},

Object.freeze(SysInfoMeasurement.prototype);


this.SysInfoProvider = function SysInfoProvider() {
  MetricsProvider.call(this, "sys-info");
};

SysInfoProvider.prototype = {
  __proto__: MetricsProvider.prototype,

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

  INT_FIELDS: new Set("cpucount", "memsize"),

  collectConstantMeasurements: function collectConstantMeasurements() {
    let result = this.createResult();
    result.expectMeasurement("sysinfo");

    result.populate = this._populateConstants.bind(this);

    return result;
  },

  _populateConstants: function _populateConstants(result) {
    result.addMeasurement(new SysInfoMeasurement());

    let si = Cc["@mozilla.org/system-info;1"]
               .getService(Ci.nsIPropertyBag2);

    for (let [k, v] in Iterator(this.sysInfoFields)) {
      try {
        if (!si.hasKey(k)) {
          this._log.debug("Property not available: " + k);
          continue;
        }

        let value = si.getProperty(k);

        if (this.INT_FIELDS.has(k)) {
          let converted = parseInt(value, 10);
          if (Number.isNaN(converted)) {
            result.addError(new Error("Value is not an integer: " + k + "=" +
                                      value));
            continue;
          }

          value = converted;
        }

        // Round memory to mebibytes.
        if (k == "memsize") {
          value = Math.round(value / 1048576);
        }

        result.setValue("sysinfo", v, value);
      } catch (ex) {
        this._log.warn("Error obtaining system info field: " + k + " " +
                       CommonUtils.exceptionStr(ex));
        result.addError(ex);
      }
    }

    result.finish();
  },
};

Object.freeze(SysInfoProvider.prototype);

