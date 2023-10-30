/**
 * @fileoverview Reject use of XPCOMUtils.generateQI and JS-implemented
 *               QueryInterface methods in favor of ChromeUtils.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

function isMemberExpression(node, object, member) {
  return (
    node.type === "MemberExpression" &&
    isIdentifier(node.object, object) &&
    isIdentifier(node.property, member)
  );
}

function funcToGenerateQI(context, node) {
  const sourceCode = context.getSourceCode();
  const text = sourceCode.getText(node);

  let interfaces = [];
  let match;
  let re = /\bCi\.([a-zA-Z0-9]+)\b|\b(nsI[A-Z][a-zA-Z0-9]+)\b/g;
  while ((match = re.exec(text))) {
    interfaces.push(match[1] || match[2]);
  }

  let ifaces = interfaces
    .filter(iface => iface != "nsISupports")
    .map(iface => JSON.stringify(iface))
    .join(", ");

  return `ChromeUtils.generateQI([${ifaces}])`;
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-chromeutils-generateqi.html",
    },
    fixable: "code",
    messages: {
      noJSQueryInterface:
        "Please use ChromeUtils.generateQI rather than " +
        "manually creating JavaScript QueryInterface functions",
      noXpcomUtilsGenerateQI:
        "Please use ChromeUtils.generateQI instead of XPCOMUtils.generateQI",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      CallExpression(node) {
        let { callee } = node;
        if (isMemberExpression(callee, "XPCOMUtils", "generateQI")) {
          context.report({
            node,
            messageId: "noXpcomUtilsGenerateQI",
            fix(fixer) {
              return fixer.replaceText(callee, "ChromeUtils.generateQI");
            },
          });
        }
      },

      "AssignmentExpression > MemberExpression[property.name='QueryInterface']":
        function (node) {
          const { right } = node.parent;
          if (right.type === "FunctionExpression") {
            context.report({
              node: node.parent,
              messageId: "noJSQueryInterface",
              fix(fixer) {
                return fixer.replaceText(
                  right,
                  funcToGenerateQI(context, right)
                );
              },
            });
          }
        },

      "Property[key.name='QueryInterface'][value.type='FunctionExpression']":
        function (node) {
          context.report({
            node,
            messageId: "noJSQueryInterface",
            fix(fixer) {
              let generateQI = funcToGenerateQI(context, node.value);
              return fixer.replaceText(node, `QueryInterface: ${generateQI}`);
            },
          });
        },
    };
  },
};
