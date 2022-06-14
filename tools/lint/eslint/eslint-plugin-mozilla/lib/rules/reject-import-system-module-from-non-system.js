/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/reject-import-system-module-from-non-system.html",
    },
    messages: {
      rejectStaticImportSystemModuleFromNonSystem:
        "System modules (*.sys.mjs) can be imported with static import declaration only from system modules.",
    },
    type: "problem",
  },

  create(context) {
    return {
      ImportDeclaration(node) {
        if (!node.source.value.endsWith(".sys.mjs")) {
          return;
        }

        context.report({
          node,
          messageId: "rejectStaticImportSystemModuleFromNonSystem",
        });
      },
    };
  },
};
