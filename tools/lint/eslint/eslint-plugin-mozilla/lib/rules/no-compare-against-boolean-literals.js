/**
 * @fileoverview Restrict comparing against `true` or `false`.
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
    BinaryExpression(node) {
      if (
        ["==", "!="].includes(node.operator) &&
        (["true", "false"].includes(node.left.raw) ||
          ["true", "false"].includes(node.right.raw))
      ) {
        context.report(
          node,
          "Don't compare for inexact equality against boolean literals"
        );
      }
    },
  };
};
