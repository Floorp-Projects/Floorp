/**
 * @fileoverview Reject multiple calls to document.l10n.formatValue in the same
 * code block.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

/**
 * As we enter blocks new sets are pushed onto this stack and then popped when
 * we exit the block.
 */
const BlockStack = [];

module.exports = {
  meta: {
    docs: {
      description: "disallow multiple document.l10n.formatValue calls",
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/prefer-formatValues.html",
    },
    messages: {
      outsideCallBlock: "call expression found outside of known block",
      useSingleCall:
        "prefer to use a single document.l10n.formatValues call instead " +
        "of multiple calls to document.l10n.formatValue or document.l10n.formatValues",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    function enterBlock() {
      BlockStack.push(new Set());
    }

    function exitBlock() {
      let calls = BlockStack.pop();
      if (calls.size > 1) {
        for (let callNode of calls) {
          context.report({
            node: callNode,
            messageId: "useSingleCall",
          });
        }
      }
    }

    return {
      Program: enterBlock,
      "Program:exit": exitBlock,
      BlockStatement: enterBlock,
      "BlockStatement:exit": exitBlock,

      CallExpression(node) {
        if (!BlockStack.length) {
          context.report({
            node,
            messageId: "outsideCallBlock",
          });
        }

        let callee = node.callee;
        if (callee.type !== "MemberExpression") {
          return;
        }

        if (
          !isIdentifier(callee.property, "formatValue") &&
          !isIdentifier(callee.property, "formatValues")
        ) {
          return;
        }

        if (callee.object.type !== "MemberExpression") {
          return;
        }

        if (
          !isIdentifier(callee.object.object, "document") ||
          !isIdentifier(callee.object.property, "l10n")
        ) {
          return;
        }

        let calls = BlockStack[BlockStack.length - 1];
        calls.add(node);
      },
    };
  },
};
