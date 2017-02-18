/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env commonjs */

// A trivial module that depends on an equally trivial module
var B = require("chrome://mochitests/content/chrome/toolkit/components/workerloader/tests/moduleB-dependency.js");

// Ensure that the initial set of exports is empty
if (Object.keys(exports).length) {
  throw new Error("exports should be empty, initially");
}

// Export some values
exports.A = true;
exports.importedFoo = B.foo;
