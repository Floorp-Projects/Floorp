/**
 * @fileoverview Defines the environment when placesOverlay.xul is used
 *               as an overlay alongside scripts. Imports the globals
 *               from the relevant files.
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
var root = helpers.getRootDir(module.filename);
var modules = require(path.join(root,
                                "tools", "lint", "eslint", "modules.json"));

const placesOverlayFiles = [
  "toolkit/content/globalOverlay.js",
  "browser/base/content/utilityOverlay.js",
  "browser/components/places/content/controller.js",
  "browser/components/places/content/treeView.js"
];

const extraPlacesDefinitions = [
  // Straight definitions.
  {name: "Cc", writable: false},
  {name: "Ci", writable: false},
  {name: "Cr", writable: false},
  {name: "Cu", writable: false},
  // Via Components.utils / XPCOMUtils.defineLazyModuleGetter (and map to
  // single) variable.
  {name: "XPCOMUtils", writable: false},
  {name: "Task", writable: false},
  {name: "PlacesUIUtils", writable: false},
  {name: "PlacesTransactions", writable: false}
];

const placesOverlayModules = [
  "PlacesUtils.jsm"
];

function getScriptGlobals() {
  let fileGlobals = [];
  for (let file of placesOverlayFiles) {
    let fileName = path.join(root, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      // The file isn't present, this is probably not an m-c repo.
    }
  }

  for (let file of placesOverlayModules) {
    if (file in modules) {
      for (let globalVar of modules[file]) {
        fileGlobals.push({name: globalVar, writable: false});
      }
    }
  }

  return fileGlobals.concat(extraPlacesDefinitions);
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
