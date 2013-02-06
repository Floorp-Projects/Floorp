/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DummyMeasurement",
  "DummyProvider",
  "DummyConstantProvider",
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

  configureStorage: function () {
    let self = this;
    return Task.spawn(function configureStorage() {
      yield self.registerStorageField("daily-counter", self.storage.FIELD_DAILY_COUNTER);
      yield self.registerStorageField("daily-discrete-numeric", self.storage.FIELD_DAILY_DISCRETE_NUMERIC);
      yield self.registerStorageField("daily-discrete-text", self.storage.FIELD_DAILY_DISCRETE_TEXT);
      yield self.registerStorageField("daily-last-numeric", self.storage.FIELD_DAILY_LAST_NUMERIC);
      yield self.registerStorageField("daily-last-text", self.storage.FIELD_DAILY_LAST_TEXT);
      yield self.registerStorageField("last-numeric", self.storage.FIELD_LAST_NUMERIC);
      yield self.registerStorageField("last-text", self.storage.FIELD_LAST_TEXT);
    });
  },
};


this.DummyProvider = function DummyProvider(name="DummyProvider") {
  this.name = name;

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
  DummyProvider.call(this, "DummyConstantProvider");
}

DummyConstantProvider.prototype = {
  __proto__: DummyProvider.prototype,

  constantOnly: true,
};

