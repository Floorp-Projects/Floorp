/**
 * @fileoverview Reject calls into Ci.nsIScriptableUnicodeConverter. We're phasing this out in
 * favour of TextEncoder or TextDecoder.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    MemberExpression(node) {
      if (
        isIdentifier(node.object, "Ci") &&
        isIdentifier(node.property, "nsIScriptableUnicodeConverter")
      ) {
        context.report(
          node,
          "Ci.nsIScriptableUnicodeConverter is deprecated. You should use TextEncoder or TextDecoder instead."
        );
      }
    },
  };
};
