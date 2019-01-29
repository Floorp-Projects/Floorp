/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AtomicMap"];

this.AtomicMap = class extends Map {
  set(key, value) {
    if (this.has(key)) {
      throw new RangeError("Key already used: " + key);
    }
    super.set(key, value);
  }

  pop(key) {
    if (!super.has(key)) {
      throw new RangeError("No such key in map: " + key);
    }
    const rv = super.get(key);
    super.delete(key);
    return rv;
  }
};
