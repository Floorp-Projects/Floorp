/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "macOSPoliciesParser.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

var EXPORTED_SYMBOLS = ["macOSPoliciesParser"];

var macOSPoliciesParser = {
  readPolicies(reader) {
    let nativePolicies = reader.readPreferences();
    if (!nativePolicies) {
      return null;
    }

    nativePolicies = this.unflatten(nativePolicies);
    nativePolicies = this.removeUnknownPolicies(nativePolicies);

    // Need an extra check here so we don't
    // JSON.stringify if we aren't in debug mode
    if (log.maxLogLevel == "debug") {
      log.debug(JSON.stringify(nativePolicies, null, 2));
    }

    return nativePolicies;
  },

  removeUnknownPolicies(policies) {
    let { schema } = ChromeUtils.import(
      "resource:///modules/policies/schema.jsm"
    );

    for (let policyName of Object.keys(policies)) {
      if (!schema.properties.hasOwnProperty(policyName)) {
        log.debug(`Removing unknown policy: ${policyName}`);
        delete policies[policyName];
      }
    }

    return policies;
  },

  unflatten(input, delimiter = "__") {
    let ret = {};

    for (let key of Object.keys(input)) {
      if (!key.includes(delimiter)) {
        // Short-circuit for policies that are not specified in
        // the flat format.
        ret[key] = input[key];
        continue;
      }

      log.debug(`Unflattening policy key "${key}".`);

      let subkeys = key.split(delimiter);

      // `obj`: is the intermediate step into the unflattened
      // return object. For example, for an input:
      //
      // Foo__Bar__Baz: 5,
      //
      // when the subkey being iterated is Bar, then `obj` will be
      // the Bar object being constructed, as represented below:
      //
      // ret = {
      //   Foo = {
      //     Bar = {   <---- obj
      //       Baz: 5,
      //     }
      //   }
      // }
      let obj = ret;

      // Iterate until the second to last subkey, as the last one
      // needs special handling afterwards.
      for (let i = 0; i < subkeys.length - 1; i++) {
        let subkey = subkeys[i];

        if (!isValidSubkey(subkey)) {
          log.error(`Error in key ${key}: can't use indexes bigger than 50.`);
          continue;
        }

        if (!obj[subkey]) {
          // if this subkey hasn't been seen yet, create the object
          // for it, which could be an array if the next subkey is
          // a number.
          //
          // For example, in the following examples:
          // A)
          // Foo__Bar__0
          // Foo__Bar__1
          //
          // B)
          // Foo__Bar__Baz
          // Foo__Bar__Qux
          //
          // If the subkey being analysed right now is Bar, then in example A
          // we'll create an array to accomodate the numeric entries.
          // Otherwise, if it's example B, we'll create an object to host all
          // the named keys.
          if (Number.isInteger(Number(subkeys[i + 1]))) {
            obj[subkey] = [];
          } else {
            obj[subkey] = {};
          }
        }

        obj = obj[subkey];
      }

      let lastSubkey = subkeys[subkeys.length - 1];
      if (!isValidSubkey(lastSubkey)) {
        log.error(`Error in key ${key}: can't use indexes bigger than 50.`);
        continue;
      }

      // In the last subkey, we assign it the value by accessing the input
      // object again with the full key. For example, in the case:
      //
      // input = {"Foo__Bar__Baz": 5}
      //
      // what we're doing in practice is:
      //
      // ret["Foo"]["Bar"]["Baz"] = input["Foo__Bar__Baz"];
      // \_______ _______/   |
      //         v           |
      //        obj      last subkey

      obj[lastSubkey] = input[key];
    }

    return ret;
  },
};

function isValidSubkey(subkey) {
  let valueAsNumber = Number(subkey);
  return Number.isNaN(valueAsNumber) || valueAsNumber <= 50;
}
