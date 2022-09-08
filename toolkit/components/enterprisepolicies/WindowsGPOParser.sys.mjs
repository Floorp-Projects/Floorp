/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const PREF_LOGLEVEL = "browser.policies.loglevel";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "GPOParser.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

export var WindowsGPOParser = {
  readPolicies(wrk, policies) {
    let childWrk = wrk.openChild(
      "Mozilla\\" + Services.appinfo.name,
      wrk.ACCESS_READ
    );
    if (!policies) {
      policies = {};
    }
    try {
      policies = registryToObject(childWrk, policies);
    } catch (e) {
      lazy.log.error(e);
    } finally {
      childWrk.close();
    }
    // Need an extra check here so we don't
    // JSON.stringify if we aren't in debug mode
    if (lazy.log._maxLogLevel == "debug") {
      lazy.log.debug(JSON.stringify(policies, null, 2));
    }
    return policies;
  },
};

function registryToObject(wrk, policies) {
  if (!policies) {
    policies = {};
  }
  if (wrk.valueCount > 0) {
    if (wrk.getValueName(0) == "1") {
      // If the first item is 1, just assume it is an array
      let array = [];
      for (let i = 0; i < wrk.valueCount; i++) {
        array.push(readRegistryValue(wrk, wrk.getValueName(i)));
      }
      // If it's an array, it shouldn't have any children
      return array;
    }
    for (let i = 0; i < wrk.valueCount; i++) {
      let name = wrk.getValueName(i);
      let value = readRegistryValue(wrk, name);
      if (value != undefined) {
        policies[name] = value;
      }
    }
  }
  if (wrk.childCount > 0) {
    if (wrk.getChildName(0) == "1") {
      // If the first item is 1, it's an array of objects
      let array = [];
      for (let i = 0; i < wrk.childCount; i++) {
        let name = wrk.getChildName(i);
        let childWrk = wrk.openChild(name, wrk.ACCESS_READ);
        array.push(registryToObject(childWrk));
        childWrk.close();
      }
      // If it's an array, it shouldn't have any children
      return array;
    }
    for (let i = 0; i < wrk.childCount; i++) {
      let name = wrk.getChildName(i);
      let childWrk = wrk.openChild(name, wrk.ACCESS_READ);
      policies[name] = registryToObject(childWrk);
      childWrk.close();
    }
  }
  return policies;
}

function readRegistryValue(wrk, value) {
  switch (wrk.getValueType(value)) {
    case 7: // REG_MULTI_SZ
      // While we support JSON in REG_SZ and REG_MULTI_SZ, if it's REG_MULTI_SZ,
      // we know it must be JSON. So we go ahead and JSON.parse it here so it goes
      // through the schema validator.
      try {
        return JSON.parse(wrk.readStringValue(value).replace(/\0/g, "\n"));
      } catch (e) {
        lazy.log.error(`Unable to parse JSON for ${value}`);
        return undefined;
      }
    case 2: // REG_EXPAND_SZ
    case wrk.TYPE_STRING:
      return wrk.readStringValue(value);
    case wrk.TYPE_BINARY:
      return wrk.readBinaryValue(value);
    case wrk.TYPE_INT:
      return wrk.readIntValue(value);
    case wrk.TYPE_INT64:
      return wrk.readInt64Value(value);
  }
  // unknown type
  return null;
}
