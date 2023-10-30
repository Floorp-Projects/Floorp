/**
 * @fileoverview Require providing a second parameter to get*Pref
 * methods instead of using a try/catch block.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-default-preference-values.html",
    },
    messages: {
      provideDefaultValue:
        "provide a default value instead of using a try/catch block",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      TryStatement(node) {
        let types = ["Bool", "Char", "Float", "Int"];
        let methods = types.map(type => "get" + type + "Pref");
        if (
          node.block.type != "BlockStatement" ||
          node.block.body.length != 1
        ) {
          return;
        }

        let firstStm = node.block.body[0];
        if (
          firstStm.type != "ExpressionStatement" ||
          firstStm.expression.type != "AssignmentExpression" ||
          firstStm.expression.right.type != "CallExpression" ||
          firstStm.expression.right.callee.type != "MemberExpression" ||
          firstStm.expression.right.callee.property.type != "Identifier" ||
          !methods.includes(firstStm.expression.right.callee.property.name)
        ) {
          return;
        }

        context.report({
          node,
          messageId: "provideDefaultValue",
        });
      },
    };
  },
};
