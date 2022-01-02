/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["serialize"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.WEBDRIVER_BIDI)
);

/**
 * Serialize a value as a remote value.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-remote-value
 *
 * @param {Object} value
 *     Value of any type to be serialized.
 *
 * @returns {Object} Serialized representation of the value.
 */
function serialize(value /*, maxDepth, nodeDetails, knownObjects */) {
  const type = typeof value;

  let remoteValue;

  if (type == "undefined") {
    remoteValue = { type };
  } else if (Object.is(value, null)) {
    remoteValue = { type: "null" };

    // Special numbers
  } else if (Object.is(value, NaN)) {
    remoteValue = { type: "number", value: "NaN" };
  } else if (Object.is(value, -0)) {
    remoteValue = { type: "number", value: "-0" };
  } else if (Object.is(value, Infinity)) {
    remoteValue = { type: "number", value: "+Infinity" };
  } else if (Object.is(value, -Infinity)) {
    remoteValue = { type: "number", value: "-Infinity" };
  } else if (type == "bigint") {
    remoteValue = { type, value: value.toString() };

    // values that are directly represented in the serialization
  } else if (["boolean", "number", "string"].includes(type)) {
    remoteValue = { type, value };
  } else {
    logger.warn(`Unsupported type for remote value: ${value.toString()}`);
  }

  return remoteValue;
}
