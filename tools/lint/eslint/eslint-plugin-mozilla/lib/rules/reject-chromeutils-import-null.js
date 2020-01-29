/**
 * @fileoverview Reject calls to ChromeUtils.import(..., null). This allows to
 * retrieve the global object for the JSM, instead we should rely on explicitly
 * exported symbols.
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
    CallExpression(node) {
      let { callee } = node;
      if (
        isIdentifier(callee.object, "ChromeUtils") &&
        isIdentifier(callee.property, "import") &&
        node.arguments.length >= 2 &&
        node.arguments[1].type == "Literal" &&
        node.arguments[1].raw == "null"
      ) {
        context.report(
          node,
          "ChromeUtils.import should not be called with (..., null) to " +
            "retrieve the JSM global object. Rely on explicit exports instead."
        );
      }
    },
  };
};
