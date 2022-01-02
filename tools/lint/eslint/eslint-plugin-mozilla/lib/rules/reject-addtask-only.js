/**
 * @fileoverview Don't allow only() in tests
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
    CallExpression(node) {
      if (
        node.callee.object &&
        node.callee.object.callee &&
        ["add_task", "decorate_task"].includes(
          node.callee.object.callee.name
        ) &&
        node.callee.property &&
        node.callee.property.name == "only"
      ) {
        context.report({
          node,
          message: `add_task(...).only() not allowed - add an exception if this is intentional`,
          suggest: [
            {
              desc: "Remove only() call from task",
              fix: fixer =>
                fixer.replaceTextRange(
                  [node.callee.object.range[1], node.range[1]],
                  ""
                ),
            },
          ],
        });
      }
    },
  };
};
