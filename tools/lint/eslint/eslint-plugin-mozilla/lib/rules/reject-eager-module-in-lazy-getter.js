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

function isString(node) {
  return node.type === "Literal" && typeof node.value === "string";
}

function isEagerModule(resourceURI) {
  return [
    "resource://gre/modules/Services",
    "resource://gre/modules/XPCOMUtils",
    "resource://gre/modules/AppConstants",
  ].includes(resourceURI.replace(/(\.jsm|\.jsm\.js|\.js|\.sys\.mjs)$/, ""));
}

function checkEagerModule(context, node, resourceURI) {
  if (!isEagerModule(resourceURI)) {
    return;
  }
  context.report({
    node,
    messageId: "eagerModule",
    data: { uri: resourceURI },
  });
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/tools/lint/eslint/eslint-plugin-mozilla/lib/rules/reject-eager-module-in-lazy-getter.html",
    },
    messages: {
      eagerModule:
        'Module "{{uri}}" is known to be loaded early in the startup process, and should be loaded eagerly, instead of defining a lazy getter.',
    },
    type: "problem",
  },

  create(context) {
    return {
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
          callerSource === "XPCOMUtils.defineLazyModuleGetter" ||
          callerSource === "ChromeUtils.defineModuleGetter"
        ) {
          if (node.arguments.length < 3) {
            return;
          }
          const resourceURINode = node.arguments[2];
          if (!isString(resourceURINode)) {
            return;
          }
          checkEagerModule(context, node, resourceURINode.value);
        } else if (
          callerSource === "XPCOMUtils.defineLazyModuleGetters" ||
          callerSource === "ChromeUtils.defineESModuleGetters"
        ) {
          if (node.arguments.length < 2) {
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
            checkEagerModule(context, node, resourceURINode.value);
          }
        }
      },
    };
  },
};
