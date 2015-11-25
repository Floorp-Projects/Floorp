/**
 * @fileoverview Adds the filename of imported files e.g.
 * Cu.import("some/path/Blah.jsm") adds Blah to the current scope.
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
  // ---------------------------------------------------------------------------

  return {
    ExpressionStatement: function(node) {
      var source = helpers.getSource(node, context);
      var name = helpers.getVarNameFromImportSource(source);

      if (name) {
        helpers.addVarToScope(name, context);
      }
    }
  };
};
