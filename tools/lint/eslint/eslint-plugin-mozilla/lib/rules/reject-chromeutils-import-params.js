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
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-chromeutils-import-params.html",
    },
    hasSuggestions: true,
    messages: {
      importOnlyOneArg: "ChromeUtils.import only takes one argument.",
      importOnlyOneArgSuggestion: "Remove the unnecessary parameters.",
    },
    schema: [],
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
          context.report({
            node,
            messageId: "importOnlyOneArg",
            suggest: [
              {
                messageId: "importOnlyOneArgSuggestion",
                fix: fixer => {
                  return fixer.removeRange(
                    getRangeAfterArgToEnd(context, 0, node.arguments)
                  );
                },
              },
            ],
          });
        }
      },
    };
  },
};
