/**
 * @fileoverview Marks all var declarations that are not at the top level
 *               invalid.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

var helpers = require("../helpers");

module.exports = function(context) {
  //--------------------------------------------------------------------------
  // Public
  //--------------------------------------------------------------------------

  return {
    "VariableDeclaration": function(node) {
      if (node.kind === "var") {
        var ancestors = context.getAncestors();
        var parent = ancestors.pop();

        if (parent.type === "Program") {
          return;
        }

        context.report(node, "Unexpected var, use let or const instead.");
      }
    }
  };
};
