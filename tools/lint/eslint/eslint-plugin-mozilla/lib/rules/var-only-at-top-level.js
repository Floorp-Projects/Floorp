/**
 * @fileoverview Marks all var declarations that are not at the top level
 *               invalid.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/var-only-at-top-level.html",
    },
    messages: {
      unexpectedVar: "Unexpected var, use let or const instead.",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      VariableDeclaration(node) {
        if (node.kind === "var") {
          if (helpers.getIsTopLevelScript(context.getAncestors())) {
            return;
          }

          context.report({
            node,
            messageId: "unexpectedVar",
          });
        }
      },
    };
  },
};
