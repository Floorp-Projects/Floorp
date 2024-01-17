/**
 * @fileoverview Reject use of XPCOMUtils.defineLazyGetter in favor of ChromeUtils.defineLazyGetter.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

function isMemberExpression(node, object, member) {
  return (
    node.type === "MemberExpression" &&
    isIdentifier(node.object, object) &&
    isIdentifier(node.property, member)
  );
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-chromeutils-definelazygetter.html",
    },
    fixable: "code",
    messages: {
      useChromeUtilsDefineLazyGetter:
        "Please use ChromeUtils.defineLazyGetter instead of XPCOMUtils.defineLazyGetter",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type !== "MemberExpression") {
          return;
        }

        let { callee } = node;

        if (isMemberExpression(callee, "XPCOMUtils", "defineLazyGetter")) {
          context.report({
            node,
            messageId: "useChromeUtilsDefineLazyGetter",
            fix(fixer) {
              return fixer.replaceText(callee, "ChromeUtils.defineLazyGetter");
            },
          });
        }
      },
    };
  },
};
