/**
 * @fileoverview Prevent access to CPOWs in browser mochitests.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var helpers = require("../helpers");

var cpows = new Set([
  /^gBrowser\.contentWindow/,
  /^gBrowser\.contentDocument/,
  /^gBrowser\.selectedBrowser.contentWindow/,
  /^browser\.contentDocument/,
  /^window\.content/
]);

module.exports = function(context) {
  //--------------------------------------------------------------------------
  // Helpers
  //--------------------------------------------------------------------------

  function showError(context, node, identifier) {
    context.report({
      node: node,
      message: identifier + " is a possible Cross Process Object Wrapper (CPOW)."
    });
  }

  //--------------------------------------------------------------------------
  // Public
  //--------------------------------------------------------------------------

  return {
    MemberExpression: function(node) {
      if (!helpers.getIsBrowserMochitest(this)) {
        return;
      }

      var expression = context.getSource(node);

      for (var cpow of cpows) {
        if (cpow.test(expression)) {
          showError(context, node, expression);
          return;
        }
      }
      if (helpers.getIsGlobalScope(context)) {
        if (/^content\./.test(expression)) {
          showError(context, node, expression);
          return;
        }
      }
    },

    Identifier: function(node) {
      if (!helpers.getIsBrowserMochitest(this)) {
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

        showError(context, node, expression);
        return;
      }
    }
  };
};
