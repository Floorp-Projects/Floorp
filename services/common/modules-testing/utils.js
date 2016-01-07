/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TestingUtils",
];

this.TestingUtils = {
  /**
   * Perform a deep copy of an Array or Object.
   */
  deepCopy: function deepCopy(thing, noSort) {
    if (typeof(thing) != "object" || thing == null) {
      return thing;
    }

    if (Array.isArray(thing)) {
      let ret = [];
      for (let element of thing) {
        ret.push(this.deepCopy(element, noSort));
      }

      return ret;
    }

    let ret = {};
    let props = Object.keys(thing);

    if (!noSort) {
      props = props.sort();
    }

    for (let prop of props) {
      ret[prop] = this.deepCopy(thing[prop], noSort);
    }

    return ret;
  },
};
