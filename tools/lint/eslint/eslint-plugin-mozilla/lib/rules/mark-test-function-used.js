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

var helpers = require("../helpers");

module.exports = {
  // This rule currently has no messages.
  // eslint-disable-next-line eslint-plugin/prefer-message-ids
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/mark-test-function-used.html",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
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
  },
};
