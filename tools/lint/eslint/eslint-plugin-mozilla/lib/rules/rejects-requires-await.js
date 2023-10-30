/**
 * @fileoverview Ensure Assert.rejects is preceded by await.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-requires-await.html",
    },
    messages: {
      rejectRequiresAwait: "Assert.rejects needs to be preceded by await.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type === "MemberExpression") {
          let memexp = node.callee;
          if (
            memexp.object.type === "Identifier" &&
            memexp.object.name === "Assert" &&
            memexp.property.type === "Identifier" &&
            memexp.property.name === "rejects"
          ) {
            // We have ourselves an Assert.rejects.

            if (node.parent.type !== "AwaitExpression") {
              context.report({
                node,
                messageId: "rejectRequiresAwait",
              });
            }
          }
        }
      },
    };
  },
};
