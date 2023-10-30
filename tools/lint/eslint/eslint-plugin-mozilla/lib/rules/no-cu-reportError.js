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

function isConcatenation(node) {
  return node.type == "BinaryExpression" && node.operator == "+";
}

function isIdentOrMember(node) {
  return node.type == "MemberExpression" || node.type == "Identifier";
}

function isLiteralOrConcat(node) {
  return node.type == "Literal" || isConcatenation(node);
}

function replaceConcatWithComma(fixer, node) {
  let fixes = [];
  let didFixTrailingIdentifier = false;
  let recursiveFixes;
  let trailingIdentifier;
  // Deal with recursion first:
  if (isConcatenation(node.right)) {
    // Uh oh. If the RHS is a concatenation, there are parens involved,
    // e.g.:
    // console.error("literal" + (b + "literal"));
    // It's pretty much impossible to guess what to do here so bail out:
    return { fixes: [], trailingIdentifier: false };
  }
  if (isConcatenation(node.left)) {
    ({ fixes: recursiveFixes, trailingIdentifier } = replaceConcatWithComma(
      fixer,
      node.left
    ));
    fixes.push(...recursiveFixes);
  }
  // If the left is an identifier or memberexpression, and the right is a
  // literal or concatenation - or vice versa - replace a + with a comma:
  if (
    (isIdentOrMember(node.left) && isLiteralOrConcat(node.right)) ||
    (isIdentOrMember(node.right) && isLiteralOrConcat(node.left)) ||
    // Or if the rhs is a literal/concatenation, while the right-most part of
    // the lhs is also an identifier (need 2 commas either side!)
    (trailingIdentifier && isLiteralOrConcat(node.right))
  ) {
    fixes.push(
      fixer.replaceTextRange([node.left.range[1], node.right.range[0]], ", ")
    );
    didFixTrailingIdentifier = isIdentOrMember(node.right);
  }
  return { fixes, trailingIdentifier: didFixTrailingIdentifier };
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-cu-reportError.html",
    },
    fixable: "code",
    messages: {
      useConsoleError: "Please use console.error instead of Cu.reportError",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        let checkNodes = [];
        if (isCuReportError(node.callee)) {
          // Handles cases of `Cu.reportError()`.
          if (node.arguments.length > 1) {
            // TODO: Bug 1802347 For initial landing, we allow the two
            // argument form of Cu.reportError as the second argument is a stack
            // argument which is more complicated to deal with.
            return;
          }
          checkNodes = [node.callee];
        } else if (node.arguments.length >= 1) {
          // Handles cases of `.foo(Cu.reportError)`.
          checkNodes = node.arguments.filter(n => isCuReportError(n));
        }

        for (let checkNode of checkNodes) {
          context.report({
            node,
            fix: fixer => {
              let fixes = [
                fixer.replaceText(checkNode.object, "console"),
                fixer.replaceText(checkNode.property, "error"),
              ];
              // If we're adding stuff together as an argument, split
              // into multiple arguments instead:
              if (
                checkNode == node.callee &&
                isConcatenation(node.arguments[0])
              ) {
                let { fixes: recursiveFixes } = replaceConcatWithComma(
                  fixer,
                  node.arguments[0]
                );
                fixes.push(...recursiveFixes);
              }
              return fixes;
            },
            messageId: "useConsoleError",
          });
        }
      },
    };
  },
};
