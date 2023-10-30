/**
 * @fileoverview Require use of static imports where possible.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const helpers = require("../helpers");

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-static-import.html",
    },
    fixable: "code",
    messages: {
      useStaticImport:
        "Please use static import instead of ChromeUtils.importESModule",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      VariableDeclarator(node) {
        if (
          node.init?.type != "CallExpression" ||
          node.init?.callee?.type != "MemberExpression" ||
          !context.getFilename().endsWith(".sys.mjs") ||
          !helpers.isTopLevel(context.getAncestors())
        ) {
          return;
        }

        let callee = node.init.callee;

        if (
          isIdentifier(callee.object, "ChromeUtils") &&
          isIdentifier(callee.property, "importESModule") &&
          callee.parent.arguments.length == 1
        ) {
          let sourceCode = context.getSourceCode();
          let importItemSource;
          if (node.id.type != "ObjectPattern") {
            importItemSource = sourceCode.getText(node.id);
          } else {
            importItemSource = "{ ";
            let initial = true;
            for (let property of node.id.properties) {
              if (!initial) {
                importItemSource += ", ";
              }
              initial = false;
              if (property.key.name == property.value.name) {
                importItemSource += property.key.name;
              } else {
                importItemSource += `${property.key.name} as ${property.value.name}`;
              }
            }
            importItemSource += " }";
          }

          context.report({
            node: node.parent,
            messageId: "useStaticImport",
            fix(fixer) {
              return fixer.replaceText(
                node.parent,
                `import ${importItemSource} from ${sourceCode.getText(
                  callee.parent.arguments[0]
                )}`
              );
            },
          });
        }
      },
    };
  },
};
