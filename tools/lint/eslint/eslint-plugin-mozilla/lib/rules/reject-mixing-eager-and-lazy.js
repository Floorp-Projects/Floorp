/**
 * @fileoverview Reject use of lazy getters for modules that's loaded early in
 *               the startup process and not necessarily be lazy.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const helpers = require("../helpers");

function isIdentifier(node, id) {
  return node.type === "Identifier" && node.name === id;
}

function isString(node) {
  return node.type === "Literal" && typeof node.value === "string";
}

function checkMixed(loadedModules, context, node, type, resourceURI) {
  if (!loadedModules.has(resourceURI)) {
    loadedModules.set(resourceURI, type);
  }

  if (loadedModules.get(resourceURI) === type) {
    return;
  }

  context.report({
    node,
    messageId: "mixedEagerAndLazy",
    data: { uri: resourceURI },
  });
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/tools/lint/eslint/eslint-plugin-mozilla/lib/rules/reject-mixed-eager-and-lazy.html",
    },
    messages: {
      mixedEagerAndLazy:
        'Module "{{uri}}" is loaded eagerly, and should not be used for lazy getter.',
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    const loadedModules = new Map();

    return {
      ImportDeclaration(node) {
        const resourceURI = node.source.value;
        checkMixed(loadedModules, context, node, "eager", resourceURI);
      },
      CallExpression(node) {
        if (node.callee.type !== "MemberExpression") {
          return;
        }

        let callerSource;
        try {
          callerSource = helpers.getASTSource(node.callee);
        } catch (e) {
          return;
        }

        if (
          (callerSource === "ChromeUtils.import" ||
            callerSource === "ChromeUtils.importESModule") &&
          helpers.getIsTopLevelAndUnconditionallyExecuted(
            context.getAncestors()
          )
        ) {
          if (node.arguments.length < 1) {
            return;
          }
          const resourceURINode = node.arguments[0];
          if (!isString(resourceURINode)) {
            return;
          }
          checkMixed(
            loadedModules,
            context,
            node,
            "eager",
            resourceURINode.value
          );
        }

        if (callerSource === "ChromeUtils.defineModuleGetter") {
          if (node.arguments.length < 3) {
            return;
          }
          if (!isIdentifier(node.arguments[0], "lazy")) {
            return;
          }

          const resourceURINode = node.arguments[2];
          if (!isString(resourceURINode)) {
            return;
          }
          checkMixed(
            loadedModules,
            context,
            node,
            "lazy",
            resourceURINode.value
          );
        } else if (
          callerSource === "XPCOMUtils.defineLazyModuleGetters" ||
          callerSource === "ChromeUtils.defineESModuleGetters"
        ) {
          if (node.arguments.length < 2) {
            return;
          }
          if (!isIdentifier(node.arguments[0], "lazy")) {
            return;
          }

          const obj = node.arguments[1];
          if (obj.type !== "ObjectExpression") {
            return;
          }
          for (let prop of obj.properties) {
            if (prop.type !== "Property") {
              continue;
            }
            if (prop.kind !== "init") {
              continue;
            }
            const resourceURINode = prop.value;
            if (!isString(resourceURINode)) {
              continue;
            }
            checkMixed(
              loadedModules,
              context,
              node,
              "lazy",
              resourceURINode.value
            );
          }
        }
      },
    };
  },
};
