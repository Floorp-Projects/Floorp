/**
 * @fileoverview Reject common XPCOM methods called with useless optional
 *               parameters, or non-existent parameters.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    "CallExpression": function(node) {
      let callee = node.callee;
      if (callee.type === "MemberExpression" &&
          callee.object.type === "Identifier" &&
          callee.object.name === "Task") {
        context.report({node, message: "Task.jsm is deprecated."});
      }
    }
  };
};
