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

var cpows = [
  /^gBrowser\.contentWindow/,
  /^gBrowser\.contentDocument/,
  /^gBrowser\.selectedBrowser\.contentWindow/,
  /^browser\.contentDocument/,
  /^window\.content/
];

// Keep track of where the last error is reported so to avoid reporting the same
// expression (e.g., window.content.X vs window.content). Resets for each file.
var lastErrorStart;

// Keep track of whether we've entered a ContentTask.spawn call where accesses
// to "content" are *not* CPOW. Resets when exiting the call.
var isInContentTask = false;

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  function showError(node, identifier) {
    if (isInContentTask) {
      return;
    }

    // Avoid showing partial expressions errors when one already errored.
    if (node.start === lastErrorStart) {
      return;
    }
    lastErrorStart = node.start;

    context.report({
      node,
      message: identifier +
               " is a possible Cross Process Object Wrapper (CPOW)."
    });
  }

  function hasLocalContentVariable(node) {
    // Walk up the parents, see if we can find if "content" is a local variable.
    let parent = node;
    do {
      parent = parent.parent;

      // Don't error if 'content' is one of the function parameters.
      if (helpers.getIsFunctionNode(parent) &&
          context.getDeclaredVariables(parent).some(variable => variable.name === "content")) {
        return true;
      } else if (parent.type === "BlockStatement" || parent.type === "Program") {
        // Don't error if the block or program includes their own definition of content.
        for (let item of parent.body) {
          if (item.type === "VariableDeclaration" && item.declarations.length) {
            for (let declaration of item.declarations) {
              if (declaration.id && declaration.id.name === "content") {
                return true;
              }
            }
          }
        }
      }
    } while (parent.parent);
    return false;
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

    Program() {
      lastErrorStart = undefined;
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

      // Specially scope checks for "context." to avoid false positives.
      if (!someCpowFound && /^content\./.test(expression)) {
        // Don't error if we're multiple scopes deep. For now we only care about
        // 2 scopes: global (length = 0) or immediately called by add_task.
        // Ideally, we would care about any scope, but figuring out whether
        // "context" is CPOW-or-not depends on how functions are defined and
        // called -- both aspects are especially complex with head.js helpers.
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1417017#c9
        const scopes = context.getAncestors().filter(helpers.getIsFunctionNode);
        if (scopes.length > 1 || scopes.length === 1 &&
            (!scopes[0].parent.callee || scopes[0].parent.callee.name !== "add_task")) {
          return;
        }

        // Don't error if there's a locally scoped "content"
        if (hasLocalContentVariable(node)) {
          return;
        }

        showError(node, expression);
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

      // Don't error if there's a locally scoped "content"
      if (hasLocalContentVariable(node)) {
        return;
      }

      var expression = context.getSource(node);
      showError(node, expression);
    }
  };
};
