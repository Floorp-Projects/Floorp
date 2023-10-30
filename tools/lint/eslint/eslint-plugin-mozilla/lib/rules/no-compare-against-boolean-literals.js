/**
 * @fileoverview Restrict comparing against `true` or `false`.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-compare-against-boolean-literals.html",
    },
    messages: {
      noCompareBoolean:
        "Don't compare for inexact equality against boolean literals",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      BinaryExpression(node) {
        if (
          ["==", "!="].includes(node.operator) &&
          (["true", "false"].includes(node.left.raw) ||
            ["true", "false"].includes(node.right.raw))
        ) {
          context.report({
            node,
            messageId: "noCompareBoolean",
          });
        }
      },
    };
  },
};
