/**
 * @fileoverview Reject `add_task(async function setup` or similar patterns in
 * favour of add_setup.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isNamedLikeSetup(name) {
  return /^(init|setup)$/i.test(name);
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-addtask-setup.html",
    },
    fixable: "code",
    messages: {
      useAddSetup: "Do not use add_task() for setup, use add_setup() instead.",
    },
    schema: [],
    type: "suggestion",
  },
  create(context) {
    return {
      "Program > ExpressionStatement > CallExpression": function (node) {
        let callee = node.callee;
        if (callee.type === "Identifier" && callee.name === "add_task") {
          let arg = node.arguments[0];
          if (
            arg.type !== "FunctionExpression" ||
            !arg.id ||
            !isNamedLikeSetup(arg.id.name)
          ) {
            return;
          }
          context.report({
            node,
            messageId: "useAddSetup",
            fix: fixer => {
              let range = [node.callee.range[0], arg.id.range[1]];
              let asyncOrNot = arg.async ? "async " : "";
              return fixer.replaceTextRange(
                range,
                `add_setup(${asyncOrNot}function`
              );
            },
          });
        }
      },
    };
  },
};
