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

      comments.forEach(function(comment) {
        var value = comment.value.trim();
        var match = /^import-globals-from\s+(.*)$/.exec(value);

        if (match) {
          var currentFilePath = helpers.getAbsoluteFilePath(context);
          var filePath = match[1];

          if (!path.isAbsolute(filePath)) {
            var dirName = path.dirname(currentFilePath);
            filePath = path.resolve(dirName, filePath);
          }

          try {
            let globals = helpers.getGlobalsForFile(filePath);
            helpers.addGlobals(globals, context);
          } catch (e) {
            context.report(
              node,
              "Could not load globals from file {{filePath}}: {{error}}",
              {
                filePath: filePath,
                error: e
              }
            );
          }
        }
      });
    }
  };
};
