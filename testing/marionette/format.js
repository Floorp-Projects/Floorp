/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["truncate"];

const MAX_STRING_LENGTH = 250;

/**
 * Template literal that truncates string values in arbitrary objects.
 *
 * Given any object, the template will walk the object and truncate
 * any strings it comes across to a reasonable limit.  This is suitable
 * when you have arbitrary data and data integrity is not important.
 *
 * The strings are truncated in the middle so that the beginning and
 * the end is preserved.  This will make a long, truncated string look
 * like "X <...> Y", where X and Y are half the number of characters
 * of the maximum string length from either side of the string.
 *
 * Usage:
 *
 * <pre><code>
 *     truncate`Hello ${"x".repeat(260)}!`;
 *     // Hello xxx ... xxx!
 * </code></pre>
 *
 * Functions named <code>toJSON</code> or <code>toString</code>
 * on objects will be called.
 */
function truncate(strings, ...values) {
  function walk(obj) {
    const typ = Object.prototype.toString.call(obj);

    switch (typ) {
      case "[object Undefined]":
      case "[object Null]":
      case "[object Boolean]":
      case "[object Number]":
        return obj;

      case "[object String]":
        if (obj.length > MAX_STRING_LENGTH) {
          let s1 = obj.substring(0, (MAX_STRING_LENGTH / 2));
          let s2 = obj.substring(obj.length - (MAX_STRING_LENGTH / 2));
          return `${s1} ... ${s2}`;
        }
        return obj;

      case "[object Array]":
        return obj.map(walk);

      // arbitrary object
      default:
        if (Object.getOwnPropertyNames(obj).includes("toString") &&
          typeof obj.toString == "function") {
          return walk(obj.toString());
        }

        let rv = {};
        for (let prop in obj) {
          rv[prop] = walk(obj[prop]);
        }
        return rv;
    }
  }

  let res = [];
  for (let i = 0; i < strings.length; ++i) {
    res.push(strings[i]);
    if (i < values.length) {
      let obj = walk(values[i]);
      let t = Object.prototype.toString.call(obj);
      if (t == "[object Array]" || t == "[object Object]") {
        res.push(JSON.stringify(obj));
      } else {
        res.push(obj);
      }
    }
  }
  return res.join("");
}
this.truncate = truncate;
