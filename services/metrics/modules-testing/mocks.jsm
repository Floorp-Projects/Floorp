/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DummyMeasurement",
  "DummyProvider",
];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");

this.DummyMeasurement = function DummyMeasurement(name="DummyMeasurement") {
  MetricsMeasurement.call(this, name, 2);
}
DummyMeasurement.prototype = {
  __proto__: MetricsMeasurement.prototype,

  fields: {
    "string": {
      type: "TYPE_STRING",
    },

    "uint32": {
      type: "TYPE_UINT32",
      optional: true,
    },
  },
};


this.DummyProvider = function DummyProvider(name="DummyProvider") {
  MetricsProvider.call(this, name);

  this.constantMeasurementName = "DummyMeasurement";
  this.collectConstantCount = 0;
}
DummyProvider.prototype = {
  __proto__: MetricsProvider.prototype,

  collectConstantMeasurements: function collectConstantMeasurements() {
    this.collectConstantCount++;

    let result = this.createResult();
    result.expectMeasurement(this.constantMeasurementName);
    result.addMeasurement(new DummyMeasurement(this.constantMeasurementName));

    result.setValue(this.constantMeasurementName, "string", "foo");
    result.setValue(this.constantMeasurementName, "uint32", 24);

    result.finish();

    return result;
  },
};
