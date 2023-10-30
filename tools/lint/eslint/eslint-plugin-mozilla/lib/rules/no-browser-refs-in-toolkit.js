/**
 * @fileoverview Reject use of browser/-based references from code in
 *               directories like toolkit/ that ought not to depend on
 *               running inside desktop Firefox.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-browser-refs-in-toolkit.html",
    },
    messages: {
      noBrowserChrome:
        "> {{url}} is part of Desktop Firefox and cannot be unconditionally " +
        "used by this code (which has to also work elsewhere).",
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
        if (
          node.value.startsWith("chrome://browser") ||
          node.value.startsWith("resource:///") ||
          node.value.startsWith("resource://app/") ||
          (node.value.startsWith("browser/") && node.value.endsWith(".ftl"))
        ) {
          context.report({
            node,
            messageId: "noBrowserChrome",
            data: { url: node.value },
          });
        }
      },
    };
  },
};
