/**
 * @fileoverview Reject use of Cu.importGlobalProperties
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function checkArgs(context, node, name) {
  if (node.arguments.length < 2) {
    context.report({
      node,
      messageId: "needsTwoArguments",
      data: {
        name
      }
    });
    return;
  }

  if (!("regex" in node.arguments[1]) &&
      node.arguments[1].type !== "FunctionExpression" &&
      node.arguments[1].type !== "ArrowFunctionExpression" &&
      node.arguments[1].type !== "MemberExpression" &&
      (node.arguments[1].type !== "Identifier" ||
       node.arguments[1].name === "undefined")) {
    context.report({
      node,
      messageId: "requireExpected",
      data: {
        name
      }
    });
  }
}

module.exports = {
  meta: {
    messages: {
      needsTwoArguments: "Assert.{{name}} should have at least two arguments (assert, expected).",
      requireExpected: "Second argument to Assert.{{name}} should be a RegExp, function or object to compare the exception to."
    }
  },

  create(context) {
    return {
      "CallExpression": function(node) {
        if (node.callee.type === "MemberExpression") {
          let memexp = node.callee;
          if (memexp.object.type === "Identifier" &&
              memexp.object.name === "Assert" &&
              memexp.property.type === "Identifier" &&
              memexp.property.name === "rejects") {
            // We have ourselves an Assert.rejects.
            checkArgs(context, node, "rejects");
          }

          if (memexp.object.type === "Identifier" &&
              memexp.object.name === "Assert" &&
              memexp.property.type === "Identifier" &&
              memexp.property.name === "throws") {
            // We have ourselves an Assert.throws.
            checkArgs(context, node, "throws");
          }
        }
      }
    };
  }
};
