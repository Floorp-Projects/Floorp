/**
 * @fileoverview checks if/else if/else bracing is consistent
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/consistent-if-bracing.html",
    },
    messages: {
      consistentIfBracing: "Bracing of if..else bodies should be consistent.",
    },
    schema: [],
    type: "layout",
  },

  create(context) {
    return {
      IfStatement(node) {
        if (node.parent.type !== "IfStatement") {
          let types = new Set();
          for (
            let currentNode = node;
            currentNode;
            currentNode = currentNode.alternate
          ) {
            let type = currentNode.consequent.type;
            types.add(type == "BlockStatement" ? "Block" : "NotBlock");
            if (
              currentNode.alternate &&
              currentNode.alternate.type !== "IfStatement"
            ) {
              type = currentNode.alternate.type;
              types.add(type == "BlockStatement" ? "Block" : "NotBlock");
              break;
            }
          }
          if (types.size > 1) {
            context.report({
              node,
              messageId: "consistentIfBracing",
            });
          }
        }
      },
    };
  },
};
