/**
 * @fileoverview Ensures that property accesses on Services.<alias> are valid.
 * Although this largely duplicates the valid-services rule, the checks here
 * require an objdir and a manual run.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const helpers = require("../helpers");

function findInterfaceNames(name) {
  let interfaces = [];
  for (let [key, value] of Object.entries(helpers.servicesData)) {
    if (value == name) {
      interfaces.push(key);
    }
  }
  return interfaces;
}

function isInInterface(interfaceName, name) {
  let interfaceDetails = helpers.xpidlData.get(interfaceName);

  // TODO: Bug 1790261 - check only methods if the expression is callable.
  if (interfaceDetails.methods.some(m => m.name == name)) {
    return true;
  }

  if (interfaceDetails.consts.some(c => c.name == name)) {
    return true;
  }

  if (interfaceDetails.parent) {
    return isInInterface(interfaceDetails.parent, name);
  }
  return false;
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/valid-services-property.html",
    },
    messages: {
      unknownProperty:
        "Unknown property access Services.{{ alias }}.{{ propertyName }}, Interfaces: {{ checkedInterfaces }}",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    let servicesInterfaceMap = helpers.servicesData;
    let serviceAliases = new Set([
      ...Object.values(servicesInterfaceMap),
      // This is defined only for Android, so most builds won't pick it up.
      "androidBridge",
      // These are defined without interfaces and hence are not in the services map.
      "cpmm",
      "crashmanager",
      "mm",
      "ppmm",
      // The new xulStore also does not have an interface.
      "xulStore",
    ]);
    return {
      MemberExpression(node) {
        if (node.computed || node.object.type !== "Identifier") {
          return;
        }

        let mainNode;
        if (node.object.name == "Services") {
          mainNode = node;
        } else if (
          node.property.name == "Services" &&
          node.parent.type == "MemberExpression"
        ) {
          mainNode = node.parent;
        } else {
          return;
        }

        let alias = mainNode.property.name;
        if (!serviceAliases.has(alias)) {
          return;
        }

        if (
          mainNode.parent.type == "MemberExpression" &&
          !mainNode.parent.computed
        ) {
          let propertyName = mainNode.parent.property.name;
          if (propertyName == "wrappedJSObject") {
            return;
          }
          let interfaces = findInterfaceNames(alias);
          if (!interfaces.length) {
            return;
          }

          let checkedInterfaces = [];
          for (let item of interfaces) {
            if (isInInterface(item, propertyName)) {
              return;
            }
            checkedInterfaces.push(item);
          }
          context.report({
            node,
            messageId: "unknownProperty",
            data: {
              alias,
              propertyName,
              checkedInterfaces,
            },
          });
        }
      },
    };
  },
};
