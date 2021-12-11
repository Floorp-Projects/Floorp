/**
 * @fileoverview Simply marks `test` (the test method) or `run_test` as used
 * when in mochitests or xpcshell tests respectively. This avoids ESLint telling
 * us that the function is never called.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var helpers = require("../helpers");

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    Program() {
      let testType = helpers.getTestType(context);
      if (testType == "browser") {
        context.markVariableAsUsed("test");
      }

      if (testType == "xpcshell") {
        context.markVariableAsUsed("run_test");
      }

      if (helpers.getIsSjs(context)) {
        context.markVariableAsUsed("handleRequest");
      }
    },
  };
};
