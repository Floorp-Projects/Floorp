/**
 * @fileoverview Don't allow only() in tests
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-addtask-only.html",
    },
    hasSuggestions: true,
    messages: {
      addTaskNotAllowed:
        "add_task(...).only() not allowed - add an exception if this is intentional",
      addTaskNotAllowedSuggestion: "Remove only() call from task",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        if (
          ["add_task", "decorate_task"].includes(
            node.callee.object?.callee?.name
          ) &&
          node.callee.property?.name == "only"
        ) {
          context.report({
            node,
            messageId: "addTaskNotAllowed",
            suggest: [
              {
                messageId: "addTaskNotAllowedSuggestion",
                fix: fixer =>
                  fixer.replaceTextRange(
                    [node.callee.object.range[1], node.range[1]],
                    ""
                  ),
              },
            ],
          });
        }
      },
    };
  },
};
