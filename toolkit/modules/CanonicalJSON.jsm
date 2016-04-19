/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["CanonicalJSON"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "jsesc",
                                  "resource://gre/modules/third_party/jsesc/jsesc.js");

this.CanonicalJSON = {
  /**
   * Return the canonical JSON form of the passed source, sorting all the object
   * keys recursively. Note that this method will cause an infinite loop if
   * cycles exist in the source (bug 1265357).
   *
   * @param source
   *        The elements to be serialized.
   *
   * The output will have all unicode chars escaped with the unicode codepoint
   * as lowercase hexadecimal.
   *
   * @usage
   *        CanonicalJSON.stringify(listOfRecords);
   **/
  stringify: function stringify(source) {
    if (Array.isArray(source)) {
      const jsonArray = source.map(x => typeof x === "undefined" ? null : x);
      return `[${jsonArray.map(stringify).join(",")}]`;
    }

    if (typeof source === "number") {
      if (source === 0) {
        return (Object.is(source, -0)) ? "-0" : "0";
      }
    }

    // Leverage jsesc library, mainly for unicode escaping.
    const toJSON = (input) => jsesc(input, {lowercaseHex: true, json: true});

    if (typeof source !== "object" || source === null) {
      return toJSON(source);
    }

    // Dealing with objects, ordering keys.
    const sortedKeys = Object.keys(source).sort();
    const lastIndex = sortedKeys.length - 1;
    return sortedKeys.reduce((serial, key, index) => {
      const value = source[key];
      // JSON.stringify drops keys with an undefined value.
      if (typeof value === "undefined") {
        return serial;
      }
      const jsonValue = value && value.toJSON ? value.toJSON() : value;
      const suffix = index !== lastIndex ? "," : "";
      const escapedKey = toJSON(key);
      return serial + `${escapedKey}:${stringify(jsonValue)}${suffix}`;
    }, "{") + "}";
  },
};
