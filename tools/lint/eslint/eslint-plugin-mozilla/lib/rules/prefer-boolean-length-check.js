/**
 * @fileoverview Prefer boolean length check
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function funcForBooleanLength(context, node, conditionCheck) {
  let newText = "";
  const sourceCode = context.getSourceCode();
  switch (node.operator) {
    case ">":
      if (node.right.value == 0) {
        if (conditionCheck) {
          newText = sourceCode.getText(node.left);
        } else {
          newText = "!!" + sourceCode.getText(node.left);
        }
      } else {
        newText = "!" + sourceCode.getText(node.right);
      }
      break;
    case "<":
      if (node.right.value == 0) {
        newText = "!" + sourceCode.getText(node.left);
      } else if (conditionCheck) {
        newText = sourceCode.getText(node.right);
      } else {
        newText = "!!" + sourceCode.getText(node.right);
      }
      break;
    case "==":
      if (node.right.value == 0) {
        newText = "!" + sourceCode.getText(node.left);
      } else {
        newText = "!" + sourceCode.getText(node.right);
      }
      break;
    case "!=":
      if (node.right.value == 0) {
        if (conditionCheck) {
          newText = sourceCode.getText(node.left);
        } else {
          newText = "!!" + sourceCode.getText(node.left);
        }
      } else if (conditionCheck) {
        newText = sourceCode.getText(node.right);
      } else {
        newText = "!!" + sourceCode.getText(node.right);
      }
      break;
  }
  return newText;
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/prefer-boolean-length-check.html",
    },
    fixable: "code",
    messages: {
      preferBooleanCheck: "Prefer boolean length check",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    const conditionStatement = [
      "IfStatement",
      "WhileStatement",
      "DoWhileStatement",
      "ForStatement",
      "ForInStatement",
      "ConditionalExpression",
    ];

    return {
      BinaryExpression(node) {
        if (
          ["==", "!=", ">", "<"].includes(node.operator) &&
          ((node.right.type == "Literal" &&
            node.right.value == 0 &&
            node.left.property?.name == "length") ||
            (node.left.type == "Literal" &&
              node.left.value == 0 &&
              node.right.property?.name == "length"))
        ) {
          if (
            conditionStatement.includes(node.parent.type) ||
            (node.parent.type == "LogicalExpression" &&
              conditionStatement.includes(node.parent.parent.type))
          ) {
            context.report({
              node,
              fix: fixer => {
                let generateExpression = funcForBooleanLength(
                  context,
                  node,
                  true
                );

                return fixer.replaceText(node, generateExpression);
              },
              messageId: "preferBooleanCheck",
            });
          } else {
            context.report({
              node,
              fix: fixer => {
                let generateExpression = funcForBooleanLength(
                  context,
                  node,
                  false
                );
                return fixer.replaceText(node, generateExpression);
              },
              messageId: "preferBooleanCheck",
            });
          }
        }
      },
    };
  },
};
