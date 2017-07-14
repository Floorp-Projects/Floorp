/**
 * @fileoverview Disallow using Date for timing in performance sensitive code
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = {
  meta: {
    docs: {
      description: "disallow use of Date for timing measurements",
      category: "Best Practices"
    },
    schema: []
  },

  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  create(context) {
    return {
      "CallExpression": function(node) {
        let callee = node.callee;
        if (callee.type !== "MemberExpression" ||
            callee.object.type !== "Identifier" ||
            callee.object.name !== "Date" ||
            callee.property.type !== "Identifier" ||
            callee.property.name !== "now") {
          return;
        }

        context.report(node, "use performance.now() instead of Date.now() for timing " +
                             "measurements");
      },

      "NewExpression": function(node) {
        let callee = node.callee;
        if (callee.type !== "Identifier" ||
            callee.name !== "Date" ||
            node.arguments.length > 0) {
          return;
        }

        context.report(node, "use performance.now() instead of new Date() for timing " +
                             "measurements");
      }
    };
  }
};

