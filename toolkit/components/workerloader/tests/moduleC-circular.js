/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env commonjs */

// Module C and module D have circular dependencies.
// This should not prevent from loading them.

// This value is set before any circular dependency, it should be visible
// in D.
exports.enteredC = true;

var D = require("chrome://mochitests/content/chrome/toolkit/components/workerloader/tests/moduleD-circular.js");

// The following values are set after importing D.
// copiedFromD.copiedFromC should have only one field |enteredC|
exports.copiedFromD = JSON.parse(JSON.stringify(D));
// exportedFromD.copiedFromC should have all the fields defined in |exports|
exports.exportedFromD = D;
exports.finishedC = true;
