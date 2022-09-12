/**
 * @fileoverview Reject uses of unknown properties on Ci.<interface>.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const helpers = require("../helpers");

function interfaceHasProperty(interfaceName, propertyName) {
  // `Ci.nsIFoo.number` is valid, it returns the iid.
  if (propertyName == "number") {
    return true;
  }

  let interfaceInfo = helpers.xpidlData.get(interfaceName);

  // TODO: Check for missing interfaces.
  if (!interfaceInfo) {
    return true;
  }

  // If the property is not in the lists of consts for this interface, check
  // any parents as well.
  if (!interfaceInfo.consts.find(e => e.name === propertyName)) {
    if (interfaceInfo.parent && interfaceInfo.parent != "nsISupports") {
      return interfaceHasProperty(interfaceName.parent, propertyName);
    }
    return false;
  }
  return true;
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/valid-ci-uses.html",
    },
    messages: {
      unknownProperty:
        "Use of unknown property Ci.{{ interface }}.{{ property }}",
    },
    type: "problem",
  },

  create(context) {
    return {
      MemberExpression(node) {
        if (
          node.computed === false &&
          node.object.type === "MemberExpression" &&
          node.object.object.type === "Identifier" &&
          node.object.object.name === "Ci" &&
          node.object.property.type === "Identifier" &&
          node.object.property.name.includes("I") &&
          node.property.type === "Identifier"
        ) {
          if (
            !interfaceHasProperty(node.object.property.name, node.property.name)
          ) {
            context.report({
              node,
              messageId: "unknownProperty",
              data: {
                interface: node.object.property.name,
                property: node.property.name,
              },
            });
          }
        }
      },
    };
  },
};
