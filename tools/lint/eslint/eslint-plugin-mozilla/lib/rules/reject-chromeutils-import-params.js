/**
 * @fileoverview Reject calls to ChromeUtils.import(..., null). This allows to
 * retrieve the global object for the JSM, instead we should rely on explicitly
 * exported symbols.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    CallExpression(node) {
      let { callee } = node;
      if (
        isIdentifier(callee.object, "ChromeUtils") &&
        isIdentifier(callee.property, "import") &&
        node.arguments.length >= 2
      ) {
        if (
          node.arguments[1].type == "Literal" &&
          node.arguments[1].raw == "null"
        ) {
          context.report(
            node,
            "ChromeUtils.import should not be called with (..., null) to " +
              "retrieve the JSM global object. Rely on explicit exports instead."
          );
        } else if (node.arguments[1].type == "ThisExpression") {
          context.report({
            node,
            message:
              "ChromeUtils.import should not be called with (..., this) to " +
              "retrieve the JSM global object. Use destructuring instead.",
            suggest: [
              {
                desc: "Use destructuring for imports.",
                fix: fixer => {
                  let source = context.getSourceCode().getText(node);
                  let match = source.match(
                    /ChromeUtils.import\(\s*(".*\/(.*).jsm?")/m
                  );

                  return fixer.replaceText(
                    node,
                    `const { ${match[2]} } = ChromeUtils.import(${match[1]})`
                  );
                },
              },
            ],
          });
        }
      }
    },
  };
};
