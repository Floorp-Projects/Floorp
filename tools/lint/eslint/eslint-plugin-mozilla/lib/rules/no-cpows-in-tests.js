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
  /^gBrowser\.selectedBrowser.contentWindow/,
  /^browser\.contentDocument/,
  /^window\.content/
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
      node: node,
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
    CallExpression: function(node) {
      if (isContentTask(node.callee)) {
        isInContentTask = true;
      }
    },

    "CallExpression:exit": function(node) {
      if (isContentTask(node.callee)) {
        isInContentTask = false;
      }
    },

    MemberExpression: function(node) {
      if (helpers.getTestType(this) != "browser") {
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
          return;
        }
      }
    },

    Identifier: function(node) {
      if (helpers.getTestType(this) != "browser") {
        return;
      }

      var expression = context.getSource(node);
      if (expression == "content" || /^content\./.test(expression)) {
        if (node.parent.type === "MemberExpression" &&
            node.parent.object &&
            node.parent.object.type === "Identifier" &&
            node.parent.object.name != "content") {
          return;
        }
        showError(node, expression);
        return;
      }
    }
  };
};
