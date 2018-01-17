/**
 * @fileoverview Reject use of single argument Cu.import
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {

  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    "CallExpression": function(node) {
      if (node.callee.type === "MemberExpression") {
        let memexp = node.callee;
        if (memexp.object.type === "Identifier" &&
            // Only Cu and ChromeUtils, not Components.utils; see bug 1230369.
            (memexp.object.name === "Cu" ||
             memexp.object.name === "ChromeUtils") &&
            memexp.property.type === "Identifier" &&
            memexp.property.name === "import" &&
            node.arguments.length === 1) {
          context.report(node, "Single argument Cu.import exposes new " +
                               "globals to all modules");
        }
      }
    }
  };
};
