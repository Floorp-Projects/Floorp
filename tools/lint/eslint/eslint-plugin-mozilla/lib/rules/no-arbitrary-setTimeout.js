/**
 * @fileoverview Reject use of non-zero values in setTimeout
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

module.exports = {
  meta: {
    docs: {
      description: "disallow setTimeout with non-zero values in tests",
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
        // We don't want to run this on mochitests as mochitest already
        // prevents flaky setTimeout at runtime. This check is built-in
        // to the rule itself as sometimes xpcshell tests can live
        // alongside mochitests and so can't be configured via config.
        if (helpers.getTestType(context) !== "xpcshell") {
          return;
        }

        let callee = node.callee;
        if (callee.type === "MemberExpression") {
          if (callee.property.name !== "setTimeout" ||
              callee.object.name !== "window" ||
              node.arguments.length < 2) {
            return;
          }
        } else if (callee.type === "Identifier") {
          if (callee.name !== "setTimeout" ||
              node.arguments.length < 2) {
            return;
          }
        } else {
          return;
        }

        let timeout = node.arguments[1];
        if (timeout.type !== "Literal" || timeout.value > 0) {
          context.report(node, "listen for events instead of setTimeout() " +
                               "with arbitrary delay");
        }
      }
    };
  }
};
