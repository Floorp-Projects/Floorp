/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env commonjs */

// Skip a few lines
// 7
// 8
// 9
// 10
// 11
exports.doThrow = function doThrow() {
  Array.prototype.sort.apply("foo"); // This will raise a native TypeError
};
