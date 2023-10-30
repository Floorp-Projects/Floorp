/**
 * @fileoverview Warn when idempotent methods are called and their return value is unused.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-returnValue.html",
    },
    messages: {
      useReturnValue:
        "{Array/String}.{{ property }} doesn't modify the instance in-place",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      ExpressionStatement(node) {
        if (
          node.expression?.type != "CallExpression" ||
          node.expression.callee?.type != "MemberExpression" ||
          node.expression.callee.property?.type != "Identifier" ||
          !["concat", "join", "slice"].includes(
            node.expression.callee.property?.name
          )
        ) {
          return;
        }

        context.report({
          node,
          messageId: "useReturnValue",
          data: {
            property: node.expression.callee.property.name,
          },
        });
      },
    };
  },
};
