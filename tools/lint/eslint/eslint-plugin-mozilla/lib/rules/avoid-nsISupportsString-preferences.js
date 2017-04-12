/**
 * @fileoverview Rejects using getComplexValue and setComplexValue with
 *               nsISupportsString.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

function isNsISupportsString(arg) {
  let isNsISupportsStringIdentifier = obj =>
    obj.type == "Identifier" && obj.name == "nsISupportsString";
  return isNsISupportsStringIdentifier(arg) ||
         (arg.type == "MemberExpression" &&
          isNsISupportsStringIdentifier(arg.property));
}

module.exports = function(context) {

  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    "CallExpression": function(node) {
      let callee = node.callee;
      if (callee.type !== "MemberExpression" ||
          callee.property.type !== "Identifier" ||
          node.arguments.length < 2 ||
          !isNsISupportsString(node.arguments[1])) {
        return;
      }

      if (callee.property.name == "getComplexValue") {
        context.report(node, "use getStringPref instead of " +
                             "getComplexValue with nsISupportsString");
      } else if (callee.property.name == "setComplexValue") {
        context.report(node, "use setStringPref instead of " +
                             "setComplexValue with nsISupportsString");
      }
    }
  };
};
