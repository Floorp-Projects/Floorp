/**
 * @fileoverview Enforce the standard object name for
 * ChromeUtils.defineESMGetters
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isIdentifier(node, id) {
  return node.type === "Identifier" && node.name === id;
}

function calleeToString(node) {
  if (node.type === "Identifier") {
    return node.name;
  }

  if (node.type === "MemberExpression" && !node.computed) {
    return calleeToString(node.object) + "." + node.property.name;
  }

  return "???";
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-globalThis-modification.html",
    },
    messages: {
      rejectModifyGlobalThis:
        "`globalThis` shouldn't be modified. `globalThis` is the shared global inside the system module, and properties defined on it is visible from all modules.",
      rejectPassingGlobalThis:
        "`globalThis` shouldn't be passed to function that can modify it. `globalThis` is the shared global inside the system module, and properties defined on it is visible from all modules.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      AssignmentExpression(node, parents) {
        let target = node.left;
        while (target.type === "MemberExpression") {
          target = target.object;
        }
        if (isIdentifier(target, "globalThis")) {
          context.report({
            node,
            messageId: "rejectModifyGlobalThis",
          });
        }
      },
      CallExpression(node) {
        const calleeStr = calleeToString(node.callee);
        if (calleeStr.endsWith(".deserialize")) {
          return;
        }

        for (const arg of node.arguments) {
          if (isIdentifier(arg, "globalThis")) {
            context.report({
              node,
              messageId: "rejectPassingGlobalThis",
            });
          }
        }
      },
    };
  },
};
