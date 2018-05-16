/**
 * @fileoverview Reject use of Cu.importGlobalProperties
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    messages: {
      rejectRequiresAwait: "Assert.rejects needs to be preceded by await."
    }
  },

  create(context) {
    return {
      "CallExpression": function(node) {
        if (node.callee.type === "MemberExpression") {
          let memexp = node.callee;
          if (memexp.object.type === "Identifier" &&
              memexp.object.name === "Assert" &&
              memexp.property.type === "Identifier" &&
              memexp.property.name === "rejects") {
            // We have ourselves an Assert.rejects.

            if (node.parent.type !== "AwaitExpression") {
              context.report({
                node,
                messageId: "rejectRequiresAwait"
              });
            }
          }
        }
      }
    };
  }
};
