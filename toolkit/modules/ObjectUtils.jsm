/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Portions of this file are originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

"use strict";

var EXPORTED_SYMBOLS = ["ObjectUtils"];

// Used only to cause test failures.

var pSlice = Array.prototype.slice;

var ObjectUtils = {
  /**
   * This tests objects & values for deep equality.
   *
   * We check using the most exact approximation of equality between two objects
   * to keep the chance of false positives to a minimum.
   * `JSON.stringify` is not designed to be used for this purpose; objects may
   * have ambiguous `toJSON()` implementations that would influence the test.
   *
   * @param a (mixed) Object or value to be compared.
   * @param b (mixed) Object or value to be compared.
   * @return Boolean Whether the objects are deep equal.
   */
  deepEqual(a, b) {
    return _deepEqual(a, b);
  },

  /**
   * A thin wrapper on an object, designed to prevent client code from
   * accessing non-existent properties because of typos.
   *
   * // Without `strict`
   * let foo = { myProperty: 1 };
   * foo.MyProperty; // undefined
   *
   * // With `strict`
   * let strictFoo = ObjectUtils.strict(foo);
   * strictFoo.myProperty; // 1
   * strictFoo.MyProperty; // TypeError: No such property "MyProperty"
   *
   * Note that `strict` has no effect in non-DEBUG mode.
   */
  strict(obj) {
    return _strict(obj);
  },

  /**
   * Returns `true` if `obj` is an array without elements, an object without
   * enumerable properties, or a falsy primitive; `false` otherwise.
   */
  isEmpty(obj) {
    if (!obj) {
      return true;
    }
    if (typeof obj != "object") {
      return false;
    }
    if (Array.isArray(obj)) {
      return !obj.length;
    }
    for (let key in obj) {
      return false;
    }
    return true;
  },
};

// ... Start of previously MIT-licensed code.
// This deepEqual implementation is originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
// MIT license: http://opensource.org/licenses/MIT

function _deepEqual(a, b) {
  // The numbering below refers to sections in the CommonJS spec.

  // 7.1 All identical values are equivalent, as determined by ===.
  if (a === b) {
    return true;
    // 7.2 If the b value is a Date object, the a value is
    // equivalent if it is also a Date object that refers to the same time.
  }
  let aIsDate = instanceOf(a, "Date");
  let bIsDate = instanceOf(b, "Date");
  if (aIsDate || bIsDate) {
    if (!aIsDate || !bIsDate) {
      return false;
    }
    if (isNaN(a.getTime()) && isNaN(b.getTime())) {
      return true;
    }
    return a.getTime() === b.getTime();
    // 7.3 If the b value is a RegExp object, the a value is
    // equivalent if it is also a RegExp object with the same source and
    // properties (`global`, `multiline`, `lastIndex`, `ignoreCase`).
  }
  let aIsRegExp = instanceOf(a, "RegExp");
  let bIsRegExp = instanceOf(b, "RegExp");
  if (aIsRegExp || bIsRegExp) {
    return (
      aIsRegExp &&
      bIsRegExp &&
      a.source === b.source &&
      a.global === b.global &&
      a.multiline === b.multiline &&
      a.lastIndex === b.lastIndex &&
      a.ignoreCase === b.ignoreCase
    );
    // 7.4 Other pairs that do not both pass typeof value == "object",
    // equivalence is determined by ==.
  }
  if (typeof a != "object" || typeof b != "object") {
    return a == b;
  }
  // 7.5 For all other Object pairs, including Array objects, equivalence is
  // determined by having the same number of owned properties (as verified
  // with Object.prototype.hasOwnProperty.call), the same set of keys
  // (although not necessarily the same order), equivalent values for every
  // corresponding key, and an identical 'prototype' property. Note: this
  // accounts for both named and indexed properties on Arrays.
  return objEquiv(a, b);
}

function instanceOf(object, type) {
  return Object.prototype.toString.call(object) == "[object " + type + "]";
}

function isUndefinedOrNull(value) {
  return value === null || value === undefined;
}

function isArguments(object) {
  return instanceOf(object, "Arguments");
}

function objEquiv(a, b) {
  if (isUndefinedOrNull(a) || isUndefinedOrNull(b)) {
    return false;
  }
  // An identical 'prototype' property.
  if ((a.prototype || undefined) != (b.prototype || undefined)) {
    return false;
  }
  // Object.keys may be broken through screwy arguments passing. Converting to
  // an array solves the problem.
  if (isArguments(a)) {
    if (!isArguments(b)) {
      return false;
    }
    a = pSlice.call(a);
    b = pSlice.call(b);
    return _deepEqual(a, b);
  }
  let ka, kb;
  try {
    ka = Object.keys(a);
    kb = Object.keys(b);
  } catch (e) {
    // Happens when one is a string literal and the other isn't
    return false;
  }
  // Having the same number of owned properties (keys incorporates
  // hasOwnProperty)
  if (ka.length != kb.length) {
    return false;
  }
  // The same set of keys (although not necessarily the same order),
  ka.sort();
  kb.sort();
  // Equivalent values for every corresponding key, and possibly expensive deep
  // test
  for (let key of ka) {
    if (!_deepEqual(a[key], b[key])) {
      return false;
    }
  }
  return true;
}

// ... End of previously MIT-licensed code.

function _strict(obj) {
  if (typeof obj != "object") {
    throw new TypeError("Expected an object");
  }

  return new Proxy(obj, {
    get(target, name) {
      if (name in obj) {
        return obj[name];
      }

      let error = new TypeError(`No such property: "${name}"`);
      Promise.reject(error); // Cause an xpcshell/mochitest failure.
      throw error;
    },
  });
}
