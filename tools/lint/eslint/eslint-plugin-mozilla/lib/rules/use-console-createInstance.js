/**
 * @fileoverview Reject use of Console.sys.mjs and Log.sys.mjs.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-console-createInstance.html",
    },
    messages: {
      useConsoleRatherThanModule:
        "Use console.createInstance rather than {{module}}",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      Literal(node) {
        if (typeof node.value != "string") {
          return;
        }
        /* eslint-disable mozilla/use-console-createInstance */
        if (
          node.value == "resource://gre/modules/Console.sys.mjs" ||
          node.value == "resource://gre/modules/Log.sys.mjs"
        ) {
          context.report({
            node,
            messageId: "useConsoleRatherThanModule",
            data: { module: node.value.split("/").at(-1) },
          });
        }
      },
    };
  },
};
