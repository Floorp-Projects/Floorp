/**
 * @fileoverview Don't allow only() in tests
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-top-level-await.html",
    },
    messages: {
      rejectTopLevelAwait:
        "Top-level await is not currently supported in component files.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      AwaitExpression(node) {
        if (!helpers.getIsTopLevelScript(context.getAncestors())) {
          return;
        }
        context.report({ node, messageId: "rejectTopLevelAwait" });
      },
      ForOfStatement(node) {
        if (
          !node.await ||
          !helpers.getIsTopLevelScript(context.getAncestors())
        ) {
          return;
        }
        context.report({ node, messageId: "rejectTopLevelAwait" });
      },
    };
  },
};
