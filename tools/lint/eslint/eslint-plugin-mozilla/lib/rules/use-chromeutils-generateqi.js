/**
 * @fileoverview Reject use of XPCOMUtils.generateQI and JS-implemented
 *               QueryInterface methods in favor of ChromeUtils.
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

function isMemberExpression(node, object, member) {
  return (node.type === "MemberExpression" &&
          isIdentifier(node.object, object) &&
          isIdentifier(node.property, member));
}

const MSG_NO_JS_QUERY_INTERFACE = (
  "Please use ChromeUtils.generateQI rather than manually creating " +
  "JavaScript QueryInterface functions");

const MSG_NO_XPCOMUTILS_GENERATEQI = (
  "Please use ChromeUtils.generateQI instead of XPCOMUtils.generateQI");

function funcToGenerateQI(context, node) {
  const sourceCode = context.getSourceCode();
  const text = sourceCode.getText(node);

  let interfaces = [];
  let match;
  let re = /\bCi\.([a-zA-Z0-9]+)\b|\b(nsI[A-Z][a-zA-Z0-9]+)\b/g;
  while ((match = re.exec(text))) {
    interfaces.push(match[1] || match[2]);
  }

  let ifaces = interfaces.filter(iface => iface != "nsISupports")
                         .map(iface => JSON.stringify(iface))
                         .join(", ");

  return `ChromeUtils.generateQI([${ifaces}])`;
}

module.exports = {
  meta: {
    fixable: "code"
  },

  create(context) {
    return {
      "CallExpression": function(node) {
        let {callee} = node;
        if (isMemberExpression(callee, "XPCOMUtils", "generateQI")) {
          context.report({
            node,
            message: MSG_NO_XPCOMUTILS_GENERATEQI,
            fix(fixer) {
              return fixer.replaceText(callee, "ChromeUtils.generateQI");
            }
          });
        }
      },

      "AssignmentExpression > MemberExpression[property.name='QueryInterface']": function(node) {
        const {right} = node.parent;
        if (right.type === "FunctionExpression") {
          context.report({
            node: node.parent,
            message: MSG_NO_JS_QUERY_INTERFACE,
            fix(fixer) {
              return fixer.replaceText(right, funcToGenerateQI(context, right));
            }
          });
        }
      },

      "Property[key.name='QueryInterface'][value.type='FunctionExpression']": function(node) {
        context.report({
          node,
          message: MSG_NO_JS_QUERY_INTERFACE,
          fix(fixer) {
            let generateQI = funcToGenerateQI(context, node.value);
            return fixer.replaceText(node, `QueryInterface: ${generateQI}`);
          }
        });
      }
    };
  }
};

