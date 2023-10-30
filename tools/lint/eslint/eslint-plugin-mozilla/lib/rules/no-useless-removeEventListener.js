/**
 * @fileoverview Reject calls to removeEventListenter where {once: true} could
 *               be used instead.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-useless-removeEventListener.html",
    },
    messages: {
      useOnce:
        "use {once: true} instead of removeEventListener as the first instruction of the listener",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        let callee = node.callee;
        if (
          callee.type !== "MemberExpression" ||
          callee.property.type !== "Identifier" ||
          callee.property.name !== "addEventListener" ||
          node.arguments.length == 4
        ) {
          return;
        }

        let listener = node.arguments[1];
        if (
          !listener ||
          listener.type != "FunctionExpression" ||
          !listener.body ||
          listener.body.type != "BlockStatement" ||
          !listener.body.body.length ||
          listener.body.body[0].type != "ExpressionStatement" ||
          listener.body.body[0].expression.type != "CallExpression"
        ) {
          return;
        }

        let call = listener.body.body[0].expression;
        if (
          call.callee.type == "MemberExpression" &&
          call.callee.property.type == "Identifier" &&
          call.callee.property.name == "removeEventListener" &&
          ((call.arguments[0].type == "Literal" &&
            call.arguments[0].value == node.arguments[0].value) ||
            (call.arguments[0].type == "Identifier" &&
              call.arguments[0].name == node.arguments[0].name))
        ) {
          context.report({
            node: call,
            messageId: "useOnce",
          });
        }
      },
    };
  },
};
