/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

exports.B = true;
exports.foo = "foo";

// Side-effect to detect if we attempt to re-execute this module.
if ("loadedB" in self) {
  throw new Error("B has been evaluted twice");
}
self.loadedB = true;
