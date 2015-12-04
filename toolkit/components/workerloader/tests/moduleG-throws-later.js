/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Skip a few lines
// 5
// 6
// 7
// 8
// 9
exports.doThrow = function doThrow() {
  Array.prototype.sort.apply("foo"); // This will raise a native TypeError
};