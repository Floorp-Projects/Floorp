/**
 * @fileoverview Rule to prevent throwing bare Cr.ERRORs.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = {
  meta: {
    fixable: "code",
    messages: {
      bareCR: "Do not throw bare Cr.ERRORs, use Components.Exception instead",
      bareComponentsResults:
        "Do not throw bare Components.results.ERRORs, use Components.Exception instead",
    },
  },

  create(context) {
    return {
      ThrowStatement(node) {
        if (node.argument.type === "MemberExpression") {
          function fix(fixer) {
            const sourceCode = context.getSourceCode();
            const source = sourceCode.getText(node.argument);
            return fixer.replaceText(
              node.argument,
              `Components.Exception("", ${source})`
            );
          }

          const obj = node.argument.object;
          if (obj.type === "Identifier" && obj.name === "Cr") {
            context.report({
              node,
              messageId: "bareCR",
              fix,
            });
          } else if (
            obj.type === "MemberExpression" &&
            obj.object.type === "Identifier" &&
            obj.object.name === "Components" &&
            obj.property.type === "Identifier" &&
            obj.property.name === "results"
          ) {
            context.report({
              node,
              messageId: "bareComponentsResults",
              fix,
            });
          }
        }
      },
    };
  },
};
