/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

var EXPORTED_SYMBOLS = ["PreferenceFilters"];

var PreferenceFilters = {
  // Compare the value of a given preference. Takes a `default` value as an
  // optional argument to pass into `Preferences.get`.
  preferenceValue(prefKey, defaultValue) {
    return Preferences.get(prefKey, defaultValue);
  },

  // Compare if the preference is user set.
  preferenceIsUserSet(prefKey) {
    return Preferences.isSet(prefKey);
  },

  // Compare if the preference has _any_ value, whether it's user-set or default.
  preferenceExists(prefKey) {
    return Preferences.has(prefKey);
  },
};
