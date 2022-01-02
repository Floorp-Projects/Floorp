/**
 * @fileoverview Require use of Services.* rather than getService.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const helpers = require("../helpers");

let servicesInterfaceMap = helpers.servicesData;

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------
module.exports = {
  meta: {
    // fixable: "code",
  },
  create(context) {
    return {
      CallExpression(node) {
        if (!node.callee || !node.callee.property) {
          return;
        }

        if (
          node.callee.property.type == "Identifier" &&
          node.callee.property.name == "defineLazyServiceGetter" &&
          node.arguments.length == 4 &&
          node.arguments[3].type == "Literal" &&
          node.arguments[3].value in servicesInterfaceMap
        ) {
          let serviceName = servicesInterfaceMap[node.arguments[3].value];

          context.report(
            node,
            `Use Services.${serviceName} rather than defineLazyServiceGetter.`
          );
          return;
        }

        if (
          node.callee.property.type == "Identifier" &&
          node.callee.property.name == "defineLazyServiceGetters" &&
          node.arguments.length == 2 &&
          node.arguments[1].type == "ObjectExpression"
        ) {
          for (let property of node.arguments[1].properties) {
            if (
              property.value.type == "ArrayExpression" &&
              property.value.elements.length == 2 &&
              property.value.elements[1].value in servicesInterfaceMap
            ) {
              let serviceName =
                servicesInterfaceMap[property.value.elements[1].value];

              context.report(
                property.value,
                `Use Services.${serviceName} rather than defineLazyServiceGetters.`
              );
            }
          }
          return;
        }

        if (
          node.callee.property.type != "Identifier" ||
          node.callee.property.name != "getService" ||
          node.arguments.length != 1 ||
          !node.arguments[0].property ||
          node.arguments[0].property.type != "Identifier" ||
          !node.arguments[0].property.name ||
          !(node.arguments[0].property.name in servicesInterfaceMap)
        ) {
          return;
        }

        let serviceName = servicesInterfaceMap[node.arguments[0].property.name];
        context.report({
          node,
          message: `Use Services.${serviceName} rather than getService().`,
          // This is not enabled by default as for mochitest plain tests we
          // would need to replace with `SpecialPowers.Services.${serviceName}`.
          // At the moment we do not have an easy way to detect that.
          // fix(fixer) {
          //   let sourceCode = context.getSourceCode();
          //   return fixer.replaceTextRange(
          //     [
          //       sourceCode.getFirstToken(node.callee).range[0],
          //       sourceCode.getLastToken(node).range[1],
          //     ],
          //     `Services.${serviceName}`
          //   );
          // },
        });
      },
    };
  },
};
