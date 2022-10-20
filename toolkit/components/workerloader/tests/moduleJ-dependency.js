/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env commonjs */

exports.J = true;
exports.foo = "foo";

// Side-effect to detect if we attempt to re-execute this module.
if ("loadedJ" in self) {
  throw new Error("J has been evaluted twice");
}
self.loadedJ = true;
