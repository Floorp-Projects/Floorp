/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const helpers = require("../helpers");

const callExpressionDefinitions = [
  /^loader\.lazyGetter\((?:globalThis|window), "(\w+)"/,
  /^loader\.lazyServiceGetter\((?:globalThis|window), "(\w+)"/,
  /^loader\.lazyRequireGetter\((?:globalThis|window), "(\w+)"/,
  /^XPCOMUtils\.defineLazyGetter\((?:globalThis|window), "(\w+)"/,
  /^ChromeUtils\.defineLazyGetter\((?:globalThis|window), "(\w+)"/,
  /^ChromeUtils\.defineModuleGetter\((?:globalThis|window), "(\w+)"/,
  /^XPCOMUtils\.defineLazyPreferenceGetter\((?:globalThis|window), "(\w+)"/,
  /^XPCOMUtils\.defineLazyScriptGetter\((?:globalThis|window), "(\w+)"/,
  /^XPCOMUtils\.defineLazyServiceGetter\((?:globalThis|window), "(\w+)"/,
  /^XPCOMUtils\.defineConstant\((?:globalThis|window), "(\w+)"/,
  /^DevToolsUtils\.defineLazyGetter\((?:globalThis|window), "(\w+)"/,
  /^Object\.defineProperty\((?:globalThis|window), "(\w+)"/,
  /^Reflect\.defineProperty\((?:globalThis|window), "(\w+)"/,
  /^this\.__defineGetter__\("(\w+)"/,
];

const callExpressionMultiDefinitions = [
  "XPCOMUtils.defineLazyGlobalGetters(window,",
  "XPCOMUtils.defineLazyGlobalGetters(globalThis,",
  "XPCOMUtils.defineLazyModuleGetters(window,",
  "XPCOMUtils.defineLazyModuleGetters(globalThis,",
  "XPCOMUtils.defineLazyServiceGetters(window,",
  "XPCOMUtils.defineLazyServiceGetters(globalThis,",
  "ChromeUtils.defineESModuleGetters(window,",
  "ChromeUtils.defineESModuleGetters(globalThis,",
  "loader.lazyRequireGetter(window,",
  "loader.lazyRequireGetter(globalThis,",
];

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-lazy-imports-into-globals.html",
    },
    messages: {
      rejectLazyImportsIntoGlobals:
        "Non-system modules should not import into globalThis nor window. Prefer a lazy object holder",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        let source;
        try {
          source = helpers.getASTSource(node);
        } catch (e) {
          return;
        }

        if (
          callExpressionDefinitions.some(expr => source.match(expr)) ||
          callExpressionMultiDefinitions.some(expr => source.startsWith(expr))
        ) {
          context.report({ node, messageId: "rejectLazyImportsIntoGlobals" });
        }
      },
    };
  },
};
