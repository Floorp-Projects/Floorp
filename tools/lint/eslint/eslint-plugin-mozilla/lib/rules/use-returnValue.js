/**
 * @fileoverview Warn when idempotent methods are called and their return value is unused.
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
    "ExpressionStatement": function(node) {
      if (!node.expression ||
          node.expression.type != "CallExpression" ||
          !node.expression.callee ||
          node.expression.callee.type != "MemberExpression" ||
          !node.expression.callee.property ||
          node.expression.callee.property.type != "Identifier" ||
          (node.expression.callee.property.name != "concat" &&
           node.expression.callee.property.name != "join" &&
           node.expression.callee.property.name != "slice")) {
        return;
      }

      context.report(node,
                     `{Array/String}.${node.expression.callee.property.name} doesn't modify the instance in-place`);
    },
  };
};
