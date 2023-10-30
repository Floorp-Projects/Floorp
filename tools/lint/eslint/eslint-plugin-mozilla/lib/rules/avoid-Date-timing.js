/**
 * @fileoverview Disallow using Date for timing in performance sensitive code
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/avoid-Date-timing.html",
    },
    messages: {
      usePerfNow:
        "use performance.now() instead of Date.now() for timing measurements",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        let callee = node.callee;
        if (
          callee.type !== "MemberExpression" ||
          callee.object.type !== "Identifier" ||
          callee.object.name !== "Date" ||
          callee.property.type !== "Identifier" ||
          callee.property.name !== "now"
        ) {
          return;
        }

        context.report({
          node,
          messageId: "usePerfNow",
        });
      },

      NewExpression(node) {
        let callee = node.callee;
        if (
          callee.type !== "Identifier" ||
          callee.name !== "Date" ||
          node.arguments.length
        ) {
          return;
        }

        context.report({
          node,
          messageId: "usePerfNow",
        });
      },
    };
  },
};
