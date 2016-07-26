/**
 * @fileoverview Reject some uses of require.
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

  if (typeof(context.options[0]) !== "string") {
    throw new Error("reject-some-requires expects a regexp");
  }
  const RX = new RegExp(context.options[0]);

  const checkPath = function(node, path) {
    if (RX.test(path)) {
      context.report(node, `require(${path}) is not allowed`);
    }
  };

  return {
    "CallExpression": function(node) {
      if (node.callee.type == "Identifier" &&
          node.callee.name == "require" &&
          node.arguments.length == 1 &&
          node.arguments[0].type == "Literal") {
        checkPath(node, node.arguments[0].value);
      } else if (node.callee.type == "MemberExpression" &&
                 node.callee.property.type == "Identifier" &&
                 node.callee.property.name == "lazyRequireGetter" &&
                 node.arguments.length >= 3 &&
                 node.arguments[2].type == "Literal") {
        checkPath(node, node.arguments[2].value);
      }
    }
  };
};
