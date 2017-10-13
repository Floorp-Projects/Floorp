/**
 * @fileoverview Prevent access to CPOWs in browser mochitests.
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

// Note: we match to the end of the string as well as the beginning, to avoid
// multiple reports from MemberExpression statements.
var cpows = [
  /^gBrowser\.contentWindow$/,
  /^gBrowser\.contentDocument$/,
  /^gBrowser\.selectedBrowser\.contentWindow$/,
  /^browser\.contentDocument$/,
  /^window\.content$/
];

var isInContentTask = false;

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  function showError(node, identifier) {
    if (isInContentTask) {
      return;
    }

    context.report({
      node,
      message: identifier +
               " is a possible Cross Process Object Wrapper (CPOW)."
    });
  }

  function isContentTask(node) {
    return node &&
           node.type === "MemberExpression" &&
           node.property.type === "Identifier" &&
           node.property.name === "spawn" &&
           node.object.type === "Identifier" &&
           node.object.name === "ContentTask";
  }

  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    CallExpression(node) {
      if (isContentTask(node.callee)) {
        isInContentTask = true;
      }
    },

    "CallExpression:exit": function(node) {
      if (isContentTask(node.callee)) {
        isInContentTask = false;
      }
    },

    MemberExpression(node) {
      if (helpers.getTestType(context) != "browser") {
        return;
      }

      var expression = context.getSource(node);

      // Only report a single CPOW error per node -- so if checking
      // |cpows| reports one, don't report another below.
      var someCpowFound = cpows.some(function(cpow) {
        if (cpow.test(expression)) {
          showError(node, expression);
          return true;
        }
        return false;
      });
      if (!someCpowFound && helpers.getIsGlobalScope(context.getAncestors())) {
        if (/^content\./.test(expression)) {
          showError(node, expression);
        }
      }
    },

    Identifier(node) {
      if (helpers.getTestType(context) != "browser") {
        return;
      }

      if (node.name !== "content" ||
          // Don't complain if this is part of a member expression - the
          // MemberExpression() function will handle those.
          node.parent.type === "MemberExpression" ||
          // If this is a declared variable in a function, then don't complain.
          node.parent.type === "FunctionDeclaration") {
        return;
      }

      // Don't error in the case of `let content = foo`.
      if (node.parent.type === "VariableDeclarator" &&
          node.parent.id && node.parent.id.name === "content") {
        return;
      }

      // Walk up the parents, see if we can find if this is a local variable.
      let parent = node;
      do {
        parent = parent.parent;

        // Don't error if 'content' is one of the function parameters.
        if (parent.type === "FunctionDeclaration" &&
            context.getDeclaredVariables(parent).some(variable => variable.name === "content")) {
          return;
        } else if (parent.type === "BlockStatement" || parent.type === "Program") {
          // Don't error if the block or program includes their own definition of content.
          for (let item of parent.body) {
            if (item.type === "VariableDeclaration" && item.declarations.length) {
              for (let declaration of item.declarations) {
                if (declaration.id && declaration.id.name === "content") {
                  return;
                }
              }
            }
          }
        }
      } while (parent.parent);

      var expression = context.getSource(node);
      showError(node, expression);
    }
  };
};
