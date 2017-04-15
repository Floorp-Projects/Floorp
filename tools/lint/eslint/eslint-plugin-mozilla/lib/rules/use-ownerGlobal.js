/**
 * @fileoverview Require .ownerGlobal instead of .ownerDocument.defaultView.
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
    "MemberExpression": function(node) {
      if (node.property.type != "Identifier" ||
          node.property.name != "defaultView" ||
          node.object.type != "MemberExpression" ||
          node.object.property.type != "Identifier" ||
          node.object.property.name != "ownerDocument") {
        return;
      }

      context.report(node,
                     "use .ownerGlobal instead of .ownerDocument.defaultView");
    }
  };
};
