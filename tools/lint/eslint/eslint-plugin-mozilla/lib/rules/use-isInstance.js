/**
 * @fileoverview Reject use of instanceof against DOM interfaces
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const privilegedGlobals = Object.keys(
  require("../environments/privileged.js").globals
);

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

function pointsToDOMInterface(expression) {
  let object;
  let referee = expression;
  while (referee.type === "MemberExpression") {
    object = referee.object;
    referee = referee.property;
  }
  if (referee.type === "Identifier") {
    if (referee.name === "File" && object.name === "OS") {
      // OS.File is an exception that is not a DOM interface
      return false;
    }
    return privilegedGlobals.includes(referee.name);
  }
  return false;
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/use-isInstance.html",
    },
    fixable: "code",
    schema: [],
    type: "problem",
  },
  create(context) {
    return {
      BinaryExpression(node) {
        const { operator, right } = node;
        if (operator === "instanceof" && pointsToDOMInterface(right)) {
          context.report({
            node,
            message:
              "Please prefer .isInstance() in chrome scripts over the standard instanceof operator for DOM interfaces, " +
              "since the latter will return false when the object is created from a different context.",
            fix(fixer) {
              const sourceCode = context.getSourceCode();
              return fixer.replaceText(
                node,
                `${sourceCode.getText(right)}.isInstance(${sourceCode.getText(
                  node.left
                )})`
              );
            },
          });
        }
      },
    };
  },
};
