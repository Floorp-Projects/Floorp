/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This should be overwritten by module.exports
exports.key = "wrong value";

module.exports = {
  key: "value"
};

// This should also be overwritten by module.exports
exports.key = "another wrong value";
