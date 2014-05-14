/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "UpdateProvider",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

const DAILY_COUNTER_FIELD = {type: Metrics.Storage.FIELD_DAILY_COUNTER};
const DAILY_DISCRETE_NUMERIC_FIELD = {type: Metrics.Storage.FIELD_DAILY_DISCRETE_NUMERIC};

function UpdateMeasurement1() {
  Metrics.Measurement.call(this);
}

UpdateMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "update",
  version: 1,

  fields: {
    updateCheckStartCount: DAILY_COUNTER_FIELD,
    updateCheckSuccessCount: DAILY_COUNTER_FIELD,
    updateCheckFailedCount: DAILY_COUNTER_FIELD,
    updateCheckFailedStatuses: DAILY_DISCRETE_NUMERIC_FIELD,
    completeUpdateStartCount: DAILY_COUNTER_FIELD,
    partialUpdateStartCount: DAILY_COUNTER_FIELD,
    completeUpdateSuccessCount: DAILY_COUNTER_FIELD,
    partialUpdateSuccessCount: DAILY_COUNTER_FIELD,
    updateFailedCount: DAILY_COUNTER_FIELD,
    updateFailedStatuses: DAILY_DISCRETE_NUMERIC_FIELD,
  },
});

this.UpdateProvider = function () {
  Metrics.Provider.call(this);
};
UpdateProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.update",

  measurementTypes: [
    UpdateMeasurement1,
  ],

  recordUpdate: function (field, status) {
    let m = this.getMeasurement(UpdateMeasurement1.prototype.name,
                                UpdateMeasurement1.prototype.version);

    return this.enqueueStorageOperation(function recordUpdateFields() {
      return Task.spawn(function recordUpdateFieldsTask() {
        yield m.incrementDailyCounter(field + "Count");

        if ((field == "updateFailed" || field == "updateCheckFailed") && status) {
          yield m.addDailyDiscreteNumeric(field + "Statuses", status);
        }
      }.bind(this));
    }.bind(this));
  },
});
