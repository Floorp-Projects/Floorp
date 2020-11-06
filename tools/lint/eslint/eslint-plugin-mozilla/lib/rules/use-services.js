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
module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------
  return {
    CallExpression(node) {
      if (
        !node.callee ||
        !node.callee.property ||
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

      context.report(
        node,
        `Use Services.${serviceName} rather than getService().`
      );
    },
  };
};
