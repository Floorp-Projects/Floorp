/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MERGED_COMPARTMENT

"use strict";

this.EXPORTED_SYMBOLS = ["ProfileMetadataProvider"];

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;

Cu.import("resource://gre/modules/Metrics.jsm");

#endif

const DEFAULT_PROFILE_MEASUREMENT_NAME = "age";
const DEFAULT_PROFILE_MEASUREMENT_VERSION = 2;
const REQUIRED_UINT32_TYPE = {type: "TYPE_UINT32"};

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm")
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/ProfileAge.jsm");

/**
 * Measurements pertaining to the user's profile.
 */
// This is "version 1" of the metadata measurement - it must remain, but
// it's currently unused - see bug 1063714 comment 12 for why.
function ProfileMetadataMeasurement() {
  Metrics.Measurement.call(this);
}
ProfileMetadataMeasurement.prototype = {
  __proto__: Metrics.Measurement.prototype,

  name: DEFAULT_PROFILE_MEASUREMENT_NAME,
  version: 1,

  fields: {
    // Profile creation date. Number of days since Unix epoch.
    profileCreation: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
  },
};

// This is the current measurement - it adds the profileReset value.
function ProfileMetadataMeasurement2() {
  Metrics.Measurement.call(this);
}
ProfileMetadataMeasurement2.prototype = {
  __proto__: Metrics.Measurement.prototype,

  name: DEFAULT_PROFILE_MEASUREMENT_NAME,
  version: DEFAULT_PROFILE_MEASUREMENT_VERSION,

  fields: {
    // Profile creation date. Number of days since Unix epoch.
    profileCreation: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
    // Profile reset date. Number of days since Unix epoch.
    profileReset: {type: Metrics.Storage.FIELD_LAST_NUMERIC},
  },
};

/**
 * Turn a millisecond timestamp into a day timestamp.
 *
 * @param msec a number of milliseconds since epoch.
 * @return the number of whole days denoted by the input.
 */
function truncate(msec) {
  return Math.floor(msec / MILLISECONDS_PER_DAY);
}

/**
 * A Metrics.Provider for profile metadata, such as profile creation and
 * reset time.
 */
this.ProfileMetadataProvider = function() {
  Metrics.Provider.call(this);
}
this.ProfileMetadataProvider.prototype = {
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.profile",

  measurementTypes: [ProfileMetadataMeasurement2],

  pullOnly: true,

  getProfileDays: Task.async(function* () {
    let result = {};
    let accessor = new ProfileAge(null, this._log);

    let created = yield accessor.created;
    result["profileCreation"] = truncate(created);
    let reset = yield accessor.reset;
    if (reset) {
      result["profileReset"] = truncate(reset);
    }
    return result;
  }),

  collectConstantData: function () {
    let m = this.getMeasurement(DEFAULT_PROFILE_MEASUREMENT_NAME,
                                DEFAULT_PROFILE_MEASUREMENT_VERSION);

    return Task.spawn(function* collectConstants() {
      let days = yield this.getProfileDays();

      yield this.enqueueStorageOperation(function storeDays() {
        return Task.spawn(function* () {
          yield m.setLastNumeric("profileCreation", days["profileCreation"]);
          if (days["profileReset"]) {
            yield m.setLastNumeric("profileReset", days["profileReset"]);
          }
        });
      });
    }.bind(this));
  },
};

