/**
 * @fileoverview Import globals from head.js and from any files that were
 * imported by head.js (as far as we can correctly resolve the path).
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
var path = require("path");
var helpers = require("../helpers");

module.exports = function(context) {

  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    Program: function(node) {
      if (!helpers.getIsTest(this)) {
        return;
      }

      var currentFilePath = helpers.getAbsoluteFilePath(context);
      var dirName = path.dirname(currentFilePath);
      var fullHeadjsPath = path.resolve(dirName, "head.js");
      if (!fs.existsSync(fullHeadjsPath)) {
        return;
      }

      let globals = helpers.getGlobalsForFile(fullHeadjsPath);
      helpers.addGlobals(globals, context);

      // Also add any globals from import-globals-from comments
      var content = fs.readFileSync(fullHeadjsPath, "utf8");
      var comments = helpers.getAST(content).comments;
      helpers.addGlobalsFromComments(fullHeadjsPath, comments, node, context);
    }
  };
};
