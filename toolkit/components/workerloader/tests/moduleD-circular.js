/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Module C and module D have circular dependencies.
// This should not prevent from loading them.

exports.enteredD = true;
var C = require("chrome://mochitests/content/chrome/toolkit/components/workerloader/tests/moduleC-circular.js");
exports.copiedFromC = JSON.parse(JSON.stringify(C));
exports.exportedFromC = C;
exports.finishedD = true;
