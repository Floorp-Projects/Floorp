/**
 * @fileoverview When the "import-globals-from: <path>" comment is found in a
 * file, then all globals from the file at <path> will be imported in the
 * current scope.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var fs = require("fs");
var helpers = require("../helpers");
var path = require("path");

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    Program: function(node) {
      var comments = context.getSourceCode().getAllComments();
      var currentFilePath = helpers.getAbsoluteFilePath(context);
      helpers.addGlobalsFromComments(currentFilePath, comments, node, context);
    }
  };
};
