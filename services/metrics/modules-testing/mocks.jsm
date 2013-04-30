/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DummyMeasurement",
  "DummyProvider",
  "DummyConstantProvider",
  "DummyPullOnlyThrowsOnInitProvider",
  "DummyThrowOnInitProvider",
  "DummyThrowOnShutdownProvider",
];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.DummyMeasurement = function DummyMeasurement(name="DummyMeasurement") {
  this.name = name;

  Metrics.Measurement.call(this);
}

DummyMeasurement.prototype = {
  __proto__: Metrics.Measurement.prototype,

  version: 1,

  fields: {
    "daily-counter": {type: Metrics.Storage.FIELD_DAILY_COUNTER},
    "daily-discrete-numeric": {type: Metrics.Storage.FIELD_DAILY_DISCRETE_NUMERIC},
    "daily-discrete-text": {type: Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT},
    "daily-last-numeric": {type: Metrics.Storage.FIELD_DAILY_LAST_NUMERIC},
    "daily-last-text": {type: Metrics.Storage.FIELD_DAILY_LAST_TEXT},
    "last-numeric": {type: Metrics.Storage.FIELD_LAST_NUMERIC},
    "last-text": {type: Metrics.Storage.FIELD_LAST_TEXT},
  },
};


this.DummyProvider = function DummyProvider(name="DummyProvider") {
  Object.defineProperty(this, "name", {
    value: name,
  });

  this.measurementTypes = [DummyMeasurement];

  Metrics.Provider.call(this);

  this.constantMeasurementName = "DummyMeasurement";
  this.collectConstantCount = 0;
  this.throwDuringCollectConstantData = null;
  this.throwDuringConstantPopulate = null;

  this.collectDailyCount = 0;

  this.havePushedMeasurements = true;
}

DummyProvider.prototype = {
  __proto__: Metrics.Provider.prototype,

  name: "DummyProvider",

  collectConstantData: function () {
    this.collectConstantCount++;

    if (this.throwDuringCollectConstantData) {
      throw new Error(this.throwDuringCollectConstantData);
    }

    return this.enqueueStorageOperation(function doStorage() {
      if (this.throwDuringConstantPopulate) {
        throw new Error(this.throwDuringConstantPopulate);
      }

      let m = this.getMeasurement("DummyMeasurement", 1);
      let now = new Date();
      m.incrementDailyCounter("daily-counter", now);
      m.addDailyDiscreteNumeric("daily-discrete-numeric", 1, now);
      m.addDailyDiscreteNumeric("daily-discrete-numeric", 2, now);
      m.addDailyDiscreteText("daily-discrete-text", "foo", now);
      m.addDailyDiscreteText("daily-discrete-text", "bar", now);
      m.setDailyLastNumeric("daily-last-numeric", 3, now);
      m.setDailyLastText("daily-last-text", "biz", now);
      m.setLastNumeric("last-numeric", 4, now);
      return m.setLastText("last-text", "bazfoo", now);
    }.bind(this));
  },

  collectDailyData: function () {
    this.collectDailyCount++;

    return Promise.resolve();
  },
};


this.DummyConstantProvider = function () {
  DummyProvider.call(this, this.name);
}

DummyConstantProvider.prototype = {
  __proto__: DummyProvider.prototype,

  name: "DummyConstantProvider",

  pullOnly: true,
};

this.DummyThrowOnInitProvider = function () {
  DummyProvider.call(this, "DummyThrowOnInitProvider");

  throw new Error("Dummy Error");
};

this.DummyThrowOnInitProvider.prototype = {
  __proto__: DummyProvider.prototype,

  name: "DummyThrowOnInitProvider",
};

this.DummyPullOnlyThrowsOnInitProvider = function () {
  DummyConstantProvider.call(this);

  throw new Error("Dummy Error");
};

this.DummyPullOnlyThrowsOnInitProvider.prototype = {
  __proto__: DummyConstantProvider.prototype,

  name: "DummyPullOnlyThrowsOnInitProvider",
};

this.DummyThrowOnShutdownProvider = function () {
  DummyProvider.call(this, "DummyThrowOnShutdownProvider");
};

this.DummyThrowOnShutdownProvider.prototype = {
  __proto__: DummyProvider.prototype,

  name: "DummyThrowOnShutdownProvider",

  pullOnly: true,

  onShutdown: function () {
    throw new Error("Dummy shutdown error");
  },
};

