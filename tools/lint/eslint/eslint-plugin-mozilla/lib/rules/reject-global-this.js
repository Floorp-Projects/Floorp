/**
 * @fileoverview Reject attempts to use the global object in jsms.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

function isGlobalThis(context, node) {
  for (let ancestor of context.getAncestors()) {
    if (
      ancestor.type == "FunctionDeclaration" ||
      ancestor.type == "FunctionExpression" ||
      ancestor.type == "ClassProperty" ||
      ancestor.type == "ClassPrivateProperty" ||
      ancestor.type == "PropertyDefinition" ||
      ancestor.type == "StaticBlock"
    ) {
      return false;
    }
  }

  return true;
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/reject-global-this.html",
    },
    type: "problem",
  },

  create(context) {
    return {
      ThisExpression(node) {
        if (!isGlobalThis(context, node)) {
          return;
        }

        context.report({
          node,
          message: `JSM should not use the global this`,
        });
      },
    };
  },
};
