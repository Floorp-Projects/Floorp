/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "SyncProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

const DAILY_LAST_NUMERIC_FIELD = {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC};
const DAILY_LAST_TEXT_FIELD = {type: Metrics.Storage.FIELD_DAILY_LAST_TEXT};
const DAILY_COUNTER_FIELD = {type: Metrics.Storage.FIELD_DAILY_COUNTER};

XPCOMUtils.defineLazyModuleGetter(this, "Weave",
                                  "resource://services-sync/main.js");

function SyncMeasurement1() {
  Metrics.Measurement.call(this);
}

SyncMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "sync",
  version: 1,

  fields: {
    enabled: DAILY_LAST_NUMERIC_FIELD,
    preferredProtocol: DAILY_LAST_TEXT_FIELD,
    activeProtocol: DAILY_LAST_TEXT_FIELD,
    syncStart: DAILY_COUNTER_FIELD,
    syncSuccess: DAILY_COUNTER_FIELD,
    syncError: DAILY_COUNTER_FIELD,
  },
});

function SyncDevicesMeasurement1() {
  Metrics.Measurement.call(this);
}

SyncDevicesMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "devices",
  version: 1,

  fields: {},

  shouldIncludeField: function (name) {
    return true;
  },

  fieldType: function (name) {
    return Metrics.Storage.FIELD_DAILY_COUNTER;
  },
});

this.SyncProvider = function () {
  Metrics.Provider.call(this);
};
SyncProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.sync",

  measurementTypes: [
    SyncDevicesMeasurement1,
    SyncMeasurement1,
  ],

  _OBSERVERS: [
    "weave:service:sync:start",
    "weave:service:sync:finish",
    "weave:service:sync:error",
  ],

  postInit: function () {
    for (let o of this._OBSERVERS) {
      Services.obs.addObserver(this, o, false);
    }

    return Promise.resolve();
  },

  onShutdown: function () {
    for (let o of this._OBSERVERS) {
      Services.obs.removeObserver(this, o);
    }

    return Promise.resolve();
  },

  observe: function (subject, topic, data) {
    let field;
    switch (topic) {
      case "weave:service:sync:start":
        field = "syncStart";
        break;

      case "weave:service:sync:finish":
        field = "syncSuccess";
        break;

      case "weave:service:sync:error":
        field = "syncError";
        break;
    }

    let m = this.getMeasurement(SyncMeasurement1.prototype.name,
                                SyncMeasurement1.prototype.version);
    return this.enqueueStorageOperation(function recordSyncEvent() {
      return m.incrementDailyCounter(field);
    });
  },

  collectDailyData: function () {
    return this.storage.enqueueTransaction(this._populateDailyData.bind(this));
  },

  _populateDailyData: function* () {
    let m = this.getMeasurement(SyncMeasurement1.prototype.name,
                                SyncMeasurement1.prototype.version);

    let svc = Cc["@mozilla.org/weave/service;1"]
                .getService(Ci.nsISupports)
                .wrappedJSObject;

    let enabled = svc.enabled;
    yield m.setDailyLastNumeric("enabled", enabled ? 1 : 0);

    // preferredProtocol is constant and only changes as the client
    // evolves.
    yield m.setDailyLastText("preferredProtocol", "1.5");

    let protocol = svc.fxAccountsEnabled ? "1.5" : "1.1";
    yield m.setDailyLastText("activeProtocol", protocol);

    if (!enabled) {
      return;
    }

    // Before grabbing more information, be sure the Sync service
    // is fully initialized. This has the potential to initialize
    // Sync on the spot. This may be undesired if Sync appears to
    // be enabled but it really isn't. That responsibility should
    // be up to svc.enabled to not return false positives, however.
    yield svc.whenLoaded();

    if (Weave.Status.service != Weave.STATUS_OK) {
      return;
    }

    // Device types are dynamic. So we need to dynamically create fields if
    // they don't exist.
    let dm = this.getMeasurement(SyncDevicesMeasurement1.prototype.name,
                                 SyncDevicesMeasurement1.prototype.version);
    let devices = Weave.Service.clientsEngine.deviceTypes;
    for (let [field, count] of devices) {
      let hasField = this.storage.hasFieldFromMeasurement(dm.id, field,
                                    this.storage.FIELD_DAILY_LAST_NUMERIC);
      let fieldID;
      if (hasField) {
        fieldID = this.storage.fieldIDFromMeasurement(dm.id, field);
      } else {
        fieldID = yield this.storage.registerField(dm.id, field,
                                       this.storage.FIELD_DAILY_LAST_NUMERIC);
      }

      yield this.storage.setDailyLastNumericFromFieldID(fieldID, count);
    }
  },
});
