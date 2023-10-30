/**
 * @fileoverview Use .includes instead of .indexOf
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-includes-instead-of-indexOf.html",
    },
    messages: {
      useIncludes: "use .includes instead of .indexOf",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      BinaryExpression(node) {
        if (
          node.left.type != "CallExpression" ||
          node.left.callee.type != "MemberExpression" ||
          node.left.callee.property.type != "Identifier" ||
          node.left.callee.property.name != "indexOf"
        ) {
          return;
        }

        if (
          (["!=", "!==", "==", "==="].includes(node.operator) &&
            node.right.type == "UnaryExpression" &&
            node.right.operator == "-" &&
            node.right.argument.type == "Literal" &&
            node.right.argument.value == 1) ||
          ([">=", "<"].includes(node.operator) &&
            node.right.type == "Literal" &&
            node.right.value == 0)
        ) {
          context.report({
            node,
            messageId: "useIncludes",
          });
        }
      },
    };
  },
};
