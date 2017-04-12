/**
 * @fileoverview Reject calls to removeEventListenter where {once: true} could
 *               be used instead.
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
      if (callee.type !== "MemberExpression" ||
          callee.property.type !== "Identifier" ||
          callee.property.name !== "addEventListener" ||
          node.arguments.length == 4) {
        return;
      }

      let listener = node.arguments[1];
      if (listener.type != "FunctionExpression" || !listener.body ||
          listener.body.type != "BlockStatement" ||
          !listener.body.body.length ||
          listener.body.body[0].type != "ExpressionStatement" ||
          listener.body.body[0].expression.type != "CallExpression") {
        return;
      }

      let call = listener.body.body[0].expression;
      if (call.callee.type == "MemberExpression" &&
          call.callee.property.type == "Identifier" &&
          call.callee.property.name == "removeEventListener" &&
          ((call.arguments[0].type == "Literal" &&
            call.arguments[0].value == node.arguments[0].value) ||
           (call.arguments[0].type == "Identifier" &&
            call.arguments[0].name == node.arguments[0].name))) {
        context.report(call,
                       "use {once: true} instead of removeEventListener as " +
                       "the first instruction of the listener");
      }
    }
  };
};
