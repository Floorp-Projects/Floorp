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
var globals = require("../globals");

module.exports = function(context) {

  function importHead(path, node) {
    try {
      let stats = fs.statSync(path);
      if (!stats.isFile()) {
        return;
      }
    } catch (e) {
      return;
    }

    let newGlobals = globals.getGlobalsForFile(path);
    helpers.addGlobals(newGlobals, context.getScope());
  }

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
      importHead(path.resolve(dirName, "head.js"), node);

      if (!helpers.getIsXpcshellTest(this)) {
        return;
      }

      let names = fs.readdirSync(dirName);
      for (let name of names) {
        if (name.startsWith("head_") && name.endsWith(".js")) {
          importHead(path.resolve(dirName, name), node);
        }
      }
    }
  };
};
