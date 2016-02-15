/**
 * @fileoverview Marks "this.var = x" as top-level definition of "var".
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var helpers = require("../helpers");

module.exports = function(context) {

  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    "AssignmentExpression": function(node) {
      if (helpers.getIsGlobalScope(context) &&
          node.left.type === "MemberExpression" &&
          node.left.object.type === "ThisExpression" &&
          node.left.property.type === "Identifier") {
        helpers.addGlobals([node.left.property.name], context);
      }
    }
  };
};
