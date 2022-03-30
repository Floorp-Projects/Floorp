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

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

function getRangeAfterArgToEnd(context, argNumber, args) {
  let sourceCode = context.getSourceCode();
  return [
    sourceCode.getTokenAfter(args[argNumber]).range[0],
    args[args.length - 1].range[1],
  ];
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/reject-chromeutils-import-params.html",
    },
    hasSuggestions: true,
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        let { callee } = node;
        if (
          isIdentifier(callee.object, "ChromeUtils") &&
          isIdentifier(callee.property, "import") &&
          node.arguments.length >= 2
        ) {
          let targetObj = node.arguments[1];
          if (targetObj.type == "Literal" && targetObj.raw == "null") {
            context.report(
              node,
              "ChromeUtils.import should not be called with (..., null) to " +
                "retrieve the JSM global object. Rely on explicit exports instead."
            );
          } else if (targetObj.type == "ThisExpression") {
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
          } else if (
            targetObj.type == "ObjectExpression" &&
            targetObj.properties.length == 0
          ) {
            context.report({
              node,
              message:
                "Passing an empty object to ChromeUtils.import is unnecessary",
              suggest: [
                {
                  desc:
                    "Passing an empty object to ChromeUtils.import is " +
                    "unnecessary - remove the empty object",
                  fix: fixer => {
                    return fixer.removeRange(
                      getRangeAfterArgToEnd(context, 0, node.arguments)
                    );
                  },
                },
              ],
            });
          }
        }
      },
    };
  },
};
