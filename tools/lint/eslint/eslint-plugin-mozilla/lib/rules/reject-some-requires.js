/**
 * @fileoverview Reject some uses of require.
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

  if (typeof context.options[0] !== "string") {
    throw new Error("reject-some-requires expects a regexp");
  }
  const RX = new RegExp(context.options[0]);

  return {
    CallExpression(node) {
      const path = helpers.getDevToolsRequirePath(node);
      if (path && RX.test(path)) {
        context.report(node, `require(${path}) is not allowed`);
      }
    },
  };
};
