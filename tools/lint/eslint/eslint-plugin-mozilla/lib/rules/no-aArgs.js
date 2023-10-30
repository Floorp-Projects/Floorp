/**
 * @fileoverview warns against using hungarian notation in function arguments
 * (i.e. aArg).
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isPrefixed(name) {
  return name.length >= 2 && /^a[A-Z]/.test(name);
}

function deHungarianize(name) {
  return name.substring(1, 2).toLowerCase() + name.substring(2, name.length);
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-aArgs.html",
    },
    messages: {
      dontUseHungarian:
        "Parameter '{{name}}' uses Hungarian Notation, consider using '{{suggestion}}' instead.",
    },
    schema: [],
    type: "layout",
  },

  create(context) {
    function checkFunction(node) {
      for (var i = 0; i < node.params.length; i++) {
        var param = node.params[i];
        if (param.name && isPrefixed(param.name)) {
          var errorObj = {
            name: param.name,
            suggestion: deHungarianize(param.name),
          };
          context.report({
            node: param,
            messageId: "dontUseHungarian",
            data: errorObj,
          });
        }
      }
    }

    return {
      FunctionDeclaration: checkFunction,
      ArrowFunctionExpression: checkFunction,
      FunctionExpression: checkFunction,
    };
  },
};
