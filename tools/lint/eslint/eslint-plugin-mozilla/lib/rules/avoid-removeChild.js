/**
 * @fileoverview Reject using element.parentNode.removeChild(element) when
 *               element.remove() can be used instead.
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
      let callee = node.callee;
      if (callee.type !== "MemberExpression" ||
          callee.property.type !== "Identifier" ||
          callee.property.name != "removeChild" ||
          node.arguments.length != 1) {
        return;
      }

      if (callee.object.type == "MemberExpression" &&
          callee.object.property.type == "Identifier" &&
          callee.object.property.name == "parentNode" &&
          helpers.getASTSource(callee.object.object, context) ==
            helpers.getASTSource(node.arguments[0])) {
        context.report(node, "use element.remove() instead of " +
                             "element.parentNode.removeChild(element)");
      }

      if (node.arguments[0].type == "MemberExpression" &&
          node.arguments[0].property.type == "Identifier" &&
          node.arguments[0].property.name == "firstChild" &&
          helpers.getASTSource(callee.object, context) ==
            helpers.getASTSource(node.arguments[0].object)) {
        context.report(node, "use element.firstChild.remove() instead of " +
                             "element.removeChild(element.firstChild)");
      }
    }
  };
};
