/**
 * @fileoverview Restrict comparing against `true` or `false`.
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
    "BinaryExpression": function(node) {
      if (["==", "!="].includes(node.operator) &&
          (["true", "false"].includes(node.left.raw) ||
           ["true", "false"].includes(node.right.raw))) {
        context.report({
          node,
          message: "Don't compare for inexact equality against boolean literals",
          fix(fixer) {
            let unaryOp = "!!";
            if ((node.operator == "==" &&
                 (node.left.raw == "false" ||
                  node.right.raw == "false")) ||
                (node.operator == "!=" &&
                 (node.left.raw == "true" ||
                  node.right.raw == "true"))) {
              unaryOp = "!";
            }
            let fixings = [];
            if (["true", "false"].includes(node.left)) {
              fixings.push(fixer.removeRange([node.left.range[0], node.right.range[0]]));
              fixings.push(fixer.insertTextBefore(node.right, unaryOp));
            } else {
              fixings.push(fixer.removeRange([node.left.range[1], node.right.range[1]]));
              fixings.push(fixer.insertTextBefore(node.left, unaryOp));
            }
            return fixings;
          }
        });
      }
    }
  };
};
