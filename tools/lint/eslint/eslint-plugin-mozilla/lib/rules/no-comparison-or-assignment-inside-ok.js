/**
 * @fileoverview Don't allow accidental assignments inside `ok()`,
 *               and encourage people to use appropriate alternatives
 *               when using comparisons between 2 values.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const operatorToAssertionMap = {
  "==": "Assert.equal",
  "===": "Assert.strictEqual",
  "!=": "Assert.notEqual",
  "!==": "Assert.notStrictEqual",
  ">": "Assert.greater",
  "<": "Assert.less",
  "<=": "Assert.lessOrEqual",
  ">=": "Assert.greaterOrEqual",
};

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-comparison-or-assignment-inside-ok.html",
    },
    fixable: "code",
    messages: {
      assignment:
        "Assigning to a variable inside ok() is odd - did you mean to compare the two?",
      comparison:
        "Use dedicated assertion methods rather than ok(a {{operator}} b).",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    const exprs = new Set(["BinaryExpression", "AssignmentExpression"]);
    return {
      CallExpression(node) {
        if (node.callee.type != "Identifier" || node.callee.name != "ok") {
          return;
        }
        let firstArg = node.arguments[0];
        if (!exprs.has(firstArg.type)) {
          return;
        }
        if (firstArg.type == "AssignmentExpression") {
          context.report({
            node: firstArg,
            messageId: "assignment",
          });
        } else if (
          firstArg.type == "BinaryExpression" &&
          operatorToAssertionMap.hasOwnProperty(firstArg.operator)
        ) {
          context.report({
            node,
            messageId: "comparison",
            data: { operator: firstArg.operator },
            fix: fixer => {
              let left = context.sourceCode.getText(firstArg.left);
              let right = context.sourceCode.getText(firstArg.right);
              return [
                fixer.replaceText(firstArg, left + ", " + right),
                fixer.replaceText(
                  node.callee,
                  operatorToAssertionMap[firstArg.operator]
                ),
              ];
            },
          });
        }
      },
    };
  },
};
