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

const placesOverlayFiles = [
  "toolkit/content/globalOverlay.js",
  "browser/base/content/utilityOverlay.js"
];

const extraPlacesDefinitions = [
  // Via Components.utils, defineModuleGetter, defineLazyModuleGetters or
  // defineLazyScriptGetter (and map to
  // single) variable.
  {name: "XPCOMUtils", writable: false},
  {name: "Task", writable: false},
  {name: "PlacesUtils", writable: false},
  {name: "PlacesUIUtils", writable: false},
  {name: "PlacesTransactions", writable: false},
  {name: "ForgetAboutSite", writable: false},
  {name: "PlacesTreeView", writable: false},
  {name: "PlacesInsertionPoint", writable: false},
  {name: "PlacesController", writable: false},
  {name: "PlacesControllerDragHelper", writable: false}
];

function getScriptGlobals() {
  let fileGlobals = [];
  for (let file of placesOverlayFiles) {
    let fileName = path.join(helpers.rootDir, file);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      // The file isn't present, this is probably not an m-c repo.
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
  globals: helpers.isMozillaCentralBased() ?
    mapGlobals(getScriptGlobals()) :
    helpers.getSavedEnvironmentItems("places-overlay").globals
};
