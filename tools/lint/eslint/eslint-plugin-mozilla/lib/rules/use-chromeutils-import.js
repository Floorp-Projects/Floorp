/**
 * @fileoverview Reject use of Cu.import and XPCOMUtils.defineLazyModuleGetter
 *               in favor of ChromeUtils.
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
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/use-chromeutils-import.html",
    },
    fixable: "code",
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type !== "MemberExpression") {
          return;
        }

        let { callee } = node;

        // Is the expression starting with `Cu` or `Components.utils`?
        if (
          (isIdentifier(callee.object, "Cu") ||
            isMemberExpression(callee.object, "Components", "utils")) &&
          isIdentifier(callee.property, "import")
        ) {
          context.report({
            node,
            message: "Please use ChromeUtils.import instead of Cu.import",
            fix(fixer) {
              return fixer.replaceText(callee, "ChromeUtils.import");
            },
          });
        }

        if (
          isMemberExpression(callee, "XPCOMUtils", "defineLazyModuleGetter") &&
          node.arguments.length < 4
        ) {
          context.report({
            node,
            message:
              "Please use ChromeUtils.defineModuleGetter instead of " +
              "XPCOMUtils.defineLazyModuleGetter",
            fix(fixer) {
              return fixer.replaceText(
                callee,
                "ChromeUtils.defineModuleGetter"
              );
            },
          });
        }
      },
    };
  },
};
