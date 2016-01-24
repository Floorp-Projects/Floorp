/**
 * @fileoverview Reject use of Cu.importGlobalProperties
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
    "CallExpression": function(node) {
      if (node.callee.type === "MemberExpression") {
        let memexp = node.callee;
        if (memexp.object.type === "Identifier" &&
            // Only Cu, not Components.utils; see bug 1230369.
            memexp.object.name === "Cu" &&
            memexp.property.type === "Identifier" &&
            memexp.property.name === "importGlobalProperties") {
          context.report(node, "Unexpected call to Cu.importGlobalProperties");
        }
      }
    }
  };
};
