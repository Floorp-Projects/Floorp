/**
 * @fileoverview Reject use of Components.classes etc, prefer the shorthand instead.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

const componentsMap = {
  classes: "Cc",
  interfaces: "Ci",
  results: "Cr",
  utils: "Cu",
};

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    MemberExpression(node) {
      if (
        node.object.type === "Identifier" &&
        node.object.name === "Components" &&
        node.property.type === "Identifier" &&
        Object.getOwnPropertyNames(componentsMap).includes(node.property.name)
      ) {
        context.report(
          node,
          `Use ${componentsMap[node.property.name]} rather than Components.${
            node.property.name
          }`
        );
      }
    },
  };
};
