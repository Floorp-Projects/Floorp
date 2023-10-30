/**
 * @fileoverview Reject use of Components.classes etc, prefer the shorthand instead.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const componentsMap = {
  classes: "Cc",
  interfaces: "Ci",
  results: "Cr",
  utils: "Cu",
};

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-cc-etc.html",
    },
    fixable: "code",
    messages: {
      useCcEtc: "Use {{ shortName }} rather than Components.{{ oldName }}",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      MemberExpression(node) {
        if (
          node.object.type === "Identifier" &&
          node.object.name === "Components" &&
          node.property.type === "Identifier" &&
          Object.getOwnPropertyNames(componentsMap).includes(node.property.name)
        ) {
          context.report({
            node,
            messageId: "useCcEtc",
            data: {
              shortName: componentsMap[node.property.name],
              oldName: node.property.name,
            },
            fix: fixer =>
              fixer.replaceTextRange(
                [node.range[0], node.range[1]],
                componentsMap[node.property.name]
              ),
          });
        }
      },
    };
  },
};
