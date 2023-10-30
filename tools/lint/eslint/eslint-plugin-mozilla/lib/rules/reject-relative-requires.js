/**
 * @fileoverview Reject some uses of require.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");

const isRelativePath = function (path) {
  return path.startsWith("./") || path.startsWith("../");
};

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-relative-requires.html",
    },
    messages: {
      rejectRelativeRequires: "relative paths are not allowed with require()",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        const path = helpers.getDevToolsRequirePath(node);
        if (path && isRelativePath(path)) {
          context.report({
            node,
            messageId: "rejectRelativeRequires",
          });
        }
      },
    };
  },
};
