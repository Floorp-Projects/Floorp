/**
 * @fileoverview Rule to prevent throwing bare Cr.ERRORs.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isCr(object) {
  return object.type === "Identifier" && object.name === "Cr";
}

function isComponentsResults(object) {
  return (
    object.type === "MemberExpression" &&
    object.object.type === "Identifier" &&
    object.object.name === "Components" &&
    object.property.type === "Identifier" &&
    object.property.name === "results"
  );
}

function isNewError(argument) {
  return (
    argument.type === "NewExpression" &&
    argument.callee.type === "Identifier" &&
    argument.callee.name === "Error" &&
    argument.arguments.length === 1
  );
}

function fixT(context, node, argument, fixer) {
  const sourceText = context.getSourceCode().getText(argument);
  return fixer.replaceText(node, `Components.Exception("", ${sourceText})`);
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-throw-cr-literal.html",
    },
    fixable: "code",
    messages: {
      bareCR: "Do not throw bare Cr.ERRORs, use Components.Exception instead",
      bareComponentsResults:
        "Do not throw bare Components.results.ERRORs, use Components.Exception instead",
      newErrorCR:
        "Do not pass Cr.ERRORs to new Error(), use Components.Exception instead",
      newErrorComponentsResults:
        "Do not pass Components.results.ERRORs to new Error(), use Components.Exception instead",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      ThrowStatement(node) {
        if (node.argument.type === "MemberExpression") {
          const fix = fixT.bind(null, context, node.argument, node.argument);

          if (isCr(node.argument.object)) {
            context.report({
              node,
              messageId: "bareCR",
              fix,
            });
          } else if (isComponentsResults(node.argument.object)) {
            context.report({
              node,
              messageId: "bareComponentsResults",
              fix,
            });
          }
        } else if (isNewError(node.argument)) {
          const argument = node.argument.arguments[0];

          if (argument.type === "MemberExpression") {
            const fix = fixT.bind(null, context, node.argument, argument);

            if (isCr(argument.object)) {
              context.report({
                node,
                messageId: "newErrorCR",
                fix,
              });
            } else if (isComponentsResults(argument.object)) {
              context.report({
                node,
                messageId: "newErrorComponentsResults",
                fix,
              });
            }
          }
        }
      },
    };
  },
};
