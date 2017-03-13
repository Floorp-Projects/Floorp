/**
 * @fileoverview Defines the environment for scripts that use the SimpleTest
 *               mochitest harness. Imports the globals from the relevant files.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var path = require("path");
var helpers = require("../helpers");
var globals = require("../globals");

const simpleTestFiles = [
  "EventUtils.js",
  "MockObjects.js",
  "SimpleTest.js",
  "WindowSnapshot.js"
];
const simpleTestPath = "testing/mochitest/tests/SimpleTest";

function getScriptGlobals() {
  let fileGlobals = [];
  let root = helpers.getRootDir(module.filename);
  for (let file of simpleTestFiles) {
    let fileName = path.join(root, simpleTestPath, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      // The files may not be available in non-m-c repositories.
      return [];
    }
  }

  return fileGlobals;
}

function mapGlobals(fileGlobals) {
  var globalObjects = {};
  for (let global of fileGlobals) {
    globalObjects[global.name] = global.writable;
  }
  return globalObjects;
}

module.exports = {
  globals: mapGlobals(getScriptGlobals())
};
