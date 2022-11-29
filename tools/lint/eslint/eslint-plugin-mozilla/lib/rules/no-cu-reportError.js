/**
 * @fileoverview Reject common XPCOM methods called with useless optional
 *               parameters, or non-existent parameters.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isCuReportError(node) {
  return (
    node.type == "MemberExpression" &&
    node.object.type == "Identifier" &&
    node.object.name == "Cu" &&
    node.property.type == "Identifier" &&
    node.property.name == "reportError"
  );
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/no-cu-reportError.html",
    },
    fixable: "code",
    messages: {
      useConsoleError: "Please use console.error instead of Cu.reportError",
    },
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        let checkNode;
        if (
          node.arguments.length >= 1 &&
          node.arguments[0].type == "MemberExpression"
        ) {
          // Handles cases of `.foo(Cu.reportError)`.
          checkNode = node.arguments[0];
        } else {
          // Handles cases of `Cu.reportError()`.
          checkNode = node.callee;
        }
        if (!isCuReportError(checkNode)) {
          return;
        }

        if (checkNode == node.callee && node.arguments.length > 1) {
          // TODO: Bug 1802347 For initial landing, we allow the two
          // argument form of Cu.reportError as the second argument is a stack
          // argument which is more complicated to deal with.
          return;
        }

        context.report({
          node,
          fix: fixer => {
            return [
              fixer.replaceText(checkNode.object, "console"),
              fixer.replaceText(checkNode.property, "error"),
            ];
          },
          messageId: "useConsoleError",
        });
      },
    };
  },
};
