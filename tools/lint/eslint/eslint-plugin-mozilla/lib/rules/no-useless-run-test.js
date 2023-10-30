/**
 * @fileoverview Reject run_test() definitions where they aren't necessary.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-useless-run-test.html",
    },
    fixable: "code",
    messages: {
      noUselessRunTest:
        "Useless run_test function - only contains run_next_test; whole function can be removed",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      "Program > FunctionDeclaration": function (node) {
        if (
          node.id.name === "run_test" &&
          node.body.type === "BlockStatement" &&
          node.body.body.length === 1 &&
          node.body.body[0].type === "ExpressionStatement" &&
          node.body.body[0].expression.type === "CallExpression" &&
          node.body.body[0].expression.callee.name === "run_next_test"
        ) {
          context.report({
            node,
            fix: fixer => {
              let sourceCode = context.getSourceCode();
              let startNode;
              if (sourceCode.getCommentsBefore) {
                // ESLint 4 has getCommentsBefore.
                startNode = sourceCode.getCommentsBefore(node);
              } else if (node && node.body && node.leadingComments) {
                // This is for ESLint 3.
                startNode = node.leadingComments;
              }

              // If we have comments, we want the start node to be the comments,
              // rather than the token before the comments, so that we don't
              // remove the comments - for run_test, these are likely to be useful
              // information about the test.
              if (startNode?.length) {
                startNode = startNode[startNode.length - 1];
              } else {
                startNode = sourceCode.getTokenBefore(node);
              }

              return fixer.removeRange([
                // If there's no startNode, we fall back to zero, i.e. start of
                // file.
                startNode ? startNode.range[1] + 1 : 0,
                // We know the function is a block and it'll end with }. Normally
                // there's a new line after that, so just advance past it. This
                // may be slightly not dodgy in some cases, but covers the existing
                // cases.
                node.range[1] + 1,
              ]);
            },
            messageId: "noUselessRunTest",
          });
        }
      },
    };
  },
};
