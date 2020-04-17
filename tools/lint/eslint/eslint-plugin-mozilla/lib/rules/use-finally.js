/**
 * @fileoverview Require .finally() instead of .then(x, x)
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
    CallExpression(node) {
      if (
        node.callee.type == "MemberExpression" &&
        node.callee.property.name == "then" &&
        node.arguments.length == 2
      ) {
        let sourceCode = context.getSourceCode();
        let firstArg = sourceCode.getText(node.arguments[0]);
        let secondArg = sourceCode.getText(node.arguments[1]);
        if (
          firstArg == secondArg ||
          firstArg.replace(/\s+/g, "") == secondArg.replace(/\s+/g, "")
        ) {
          context.report({
            node,
            message:
              "Use .finally instead of passing 2 identical arguments to .then",
            fix(fixer) {
              let endNode = node.arguments[1];
              let comments = sourceCode.getCommentsBefore(endNode);
              if (comments.length) {
                endNode = comments[0];
              }
              return fixer.replaceTextRange(
                [node.callee.property.start, endNode.start],
                "finally("
              );
            },
          });
        }
      }
    },
  };
};
