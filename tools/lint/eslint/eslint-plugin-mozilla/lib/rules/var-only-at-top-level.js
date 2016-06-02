/**
 * @fileoverview Marks all var declarations that are not at the top level
 *               invalid.
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
    "VariableDeclaration": function(node) {
      if (node.kind === "var") {
        if (helpers.getIsGlobalScope(context.getAncestors())) {
          return;
        }

        context.report(node, "Unexpected var, use let or const instead.");
      }
    }
  };
};
