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
      message: "Unexpected assignment of non-Array to EXPORTED_SYMBOLS"
    });
    return;
  }

  for (let element of expression.elements) {
    context.markVariableAsUsed(element.value);
  }
}

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {
  // Ignore assignments not in the global scope, e.g. where special module
  // definitions are required due to having different ways of importing files,
  // e.g. osfile.
  function isGlobalScope() {
    return !context.getScope().upper;
  }

  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    AssignmentExpression(node, parents) {
      if (node.operator === "=" &&
          node.left.type === "MemberExpression" &&
          node.left.object.type === "ThisExpression" &&
          node.left.property.name === "EXPORTED_SYMBOLS" &&
          isGlobalScope()) {
        markArrayElementsAsUsed(context, node, node.right);
      }
    },

    VariableDeclaration(node, parents) {
      if (!isGlobalScope()) {
        return;
      }

      for (let item of node.declarations) {
        if (item.id &&
            item.id.type == "Identifier" &&
            item.id.name === "EXPORTED_SYMBOLS") {
          if (node.kind === "let") {
            // The use of 'let' isn't allowed as the lexical scope may die after
            // the script executes.
            context.report({
              node,
              message: "EXPORTED_SYMBOLS cannot be declared via `let`. Use `var` or `this.EXPORTED_SYMBOLS =`"
            });
          }

          markArrayElementsAsUsed(context, node, item.init);
        }
      }
    }
  };
};
