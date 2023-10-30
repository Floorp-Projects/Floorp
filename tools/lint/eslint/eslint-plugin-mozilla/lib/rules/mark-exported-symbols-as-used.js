/**
 * @fileoverview Simply marks exported symbols as used. Designed for use in
 * .jsm files only.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function markArrayElementsAsUsed(context, node, expression) {
  if (expression.type != "ArrayExpression") {
    context.report({
      node,
      messageId: "nonArrayAssignedToImported",
    });
    return;
  }

  for (let element of expression.elements) {
    context.markVariableAsUsed(element.value);
  }
  // Also mark EXPORTED_SYMBOLS as used.
  context.markVariableAsUsed("EXPORTED_SYMBOLS");
}

// Ignore assignments not in the global scope, e.g. where special module
// definitions are required due to having different ways of importing files,
// e.g. osfile.
function isGlobalScope(context) {
  return !context.getScope().upper;
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/mark-exported-symbols-as-used.html",
    },
    messages: {
      useLetForExported:
        "EXPORTED_SYMBOLS cannot be declared via `let`. Use `var` or `this.EXPORTED_SYMBOLS =`",
      nonArrayAssignedToImported:
        "Unexpected assignment of non-Array to EXPORTED_SYMBOLS",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      AssignmentExpression(node, parents) {
        if (
          node.operator === "=" &&
          node.left.type === "MemberExpression" &&
          node.left.object.type === "ThisExpression" &&
          node.left.property.name === "EXPORTED_SYMBOLS" &&
          isGlobalScope(context)
        ) {
          markArrayElementsAsUsed(context, node, node.right);
        }
      },

      VariableDeclaration(node, parents) {
        if (!isGlobalScope(context)) {
          return;
        }

        for (let item of node.declarations) {
          if (
            item.id &&
            item.id.type == "Identifier" &&
            item.id.name === "EXPORTED_SYMBOLS"
          ) {
            if (node.kind === "let") {
              // The use of 'let' isn't allowed as the lexical scope may die after
              // the script executes.
              context.report({
                node,
                messageId: "useLetForExported",
              });
            }

            markArrayElementsAsUsed(context, node, item.init);
          }
        }
      },
    };
  },
};
