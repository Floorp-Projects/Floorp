/**
 * @fileoverview Reject attempts to use the global object in jsms.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const helpers = require("../helpers");

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-global-this.html",
    },
    messages: {
      avoidGlobalThis: "JSM should not use the global this",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      ThisExpression(node) {
        if (!helpers.getIsGlobalThis(context.getAncestors())) {
          return;
        }

        context.report({
          node,
          messageId: "avoidGlobalThis",
        });
      },
    };
  },
};
