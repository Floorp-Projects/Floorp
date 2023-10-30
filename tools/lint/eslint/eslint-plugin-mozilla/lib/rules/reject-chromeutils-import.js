/**
 * @fileoverview Reject use of Cu.import and ChromeUtils.import
 *               in favor of ChromeUtils.importESModule.
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
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-chromeutils-import.html",
    },
    messages: {
      useImportESModule:
        "Please use ChromeUtils.importESModule instead of " +
        "ChromeUtils.import unless the module is not yet ESMified",
      useImportESModuleLazy:
        "Please use ChromeUtils.defineESModuleGetters instead of " +
        "ChromeUtils.defineModuleGetter " +
        "unless the module is not yet ESMified",
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

        if (
          (isIdentifier(callee.object, "ChromeUtils") ||
            isMemberExpression(
              callee.object,
              "SpecialPowers",
              "ChromeUtils"
            )) &&
          isIdentifier(callee.property, "import")
        ) {
          context.report({
            node,
            messageId: "useImportESModule",
          });
        }

        if (
          (isMemberExpression(callee.object, "SpecialPowers", "ChromeUtils") &&
            isIdentifier(callee.property, "defineModuleGetter")) ||
          isMemberExpression(callee, "ChromeUtils", "defineModuleGetter") ||
          isMemberExpression(callee, "XPCOMUtils", "defineLazyModuleGetters")
        ) {
          context.report({
            node,
            messageId: "useImportESModuleLazy",
          });
        }
      },
    };
  },
};
