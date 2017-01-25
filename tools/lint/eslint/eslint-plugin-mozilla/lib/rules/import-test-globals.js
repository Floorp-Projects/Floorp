/**
 * @fileoverview Import globals from common mochitest files, so that we
 * don't need to specify them individually.
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

const simpleTestFiles = [
  "EventUtils.js",
  "SimpleTest.js"
];
const simpleTestPath = "testing/mochitest/tests/SimpleTest";

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    Program: function(node) {
      let rootDir = helpers.getRootDir(context.getFilename());
      for (let file of simpleTestFiles) {
        let newGlobals =
          globals.getGlobalsForFile(path.join(rootDir, simpleTestPath, file));
        helpers.addGlobals(newGlobals, context.getScope());
      }
    }
  };
};
