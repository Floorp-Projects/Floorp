/**
 * @fileoverview Defines the environment when in the browser.xul window.
 *               Imports many globals from various files.
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
var placesGlobals = require("./places-overlay").globals;

const rootDir = helpers.rootDir;

// These are scripts not included in global-scripts.inc, but which are loaded
// via overlays.
const EXTRA_SCRIPTS = [
  "browser/base/content/nsContextMenu.js",
  "toolkit/content/contentAreaUtils.js",
  "browser/components/places/content/editBookmarkOverlay.js",
  "browser/components/downloads/content/downloads.js",
  "browser/components/downloads/content/indicator.js",
  // This gets loaded into the same scopes as browser.js via browser.xul and
  // placesOverlay.xul.
  "toolkit/content/globalOverlay.js",
  // Via editMenuOverlay.xul
  "toolkit/content/editMenuOverlay.js",
  // Via baseMenuOverlay.xul
  "browser/base/content/utilityOverlay.js"
];

// Some files in global-scripts.inc need mapping to specific locations.
const MAPPINGS = {
  "printUtils.js": "toolkit/components/printing/content/printUtils.js",
  "browserPlacesViews.js":
    "browser/components/places/content/browserPlacesViews.js",
  "panelUI.js": "browser/components/customizableui/content/panelUI.js",
  "viewSourceUtils.js":
    "toolkit/components/viewsource/content/viewSourceUtils.js"
};

const globalScriptsRegExp =
  /<script type=\"application\/javascript\" src=\"(.*)\"\/>/;

function getGlobalScriptsIncludes() {
  let fileData;
  try {
    fileData = fs.readFileSync(helpers.globalScriptsPath, {encoding: "utf8"});
  } catch (ex) {
    // The file isn't present, so this isn't an m-c repository.
    return null;
  }

  fileData = fileData.split("\n");

  let result = [];

  for (let line of fileData) {
    let match = line.match(globalScriptsRegExp);
    if (match) {
      let sourceFile =
        match[1].replace("chrome://browser/content/", "browser/base/content/")
                .replace("chrome://global/content/", "toolkit/content/");

      for (let mapping of Object.getOwnPropertyNames(MAPPINGS)) {
        if (sourceFile.includes(mapping)) {
          sourceFile = MAPPINGS[mapping];
        }
      }

      result.push(sourceFile);
    }
  }

  return result;
}

function getScriptGlobals() {
  let fileGlobals = [];
  let scripts = getGlobalScriptsIncludes();
  if (!scripts) {
    return [];
  }

  for (let script of scripts.concat(EXTRA_SCRIPTS)) {
    let fileName = path.join(rootDir, script);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      console.error(`Could not load globals from file ${fileName}: ${e}`);
      console.error(
        `You may need to update the mappings in ${module.filename}`);
      throw new Error(`Could not load globals from file ${fileName}: ${e}`);
    }
  }

  return fileGlobals;
}

function mapGlobals(fileGlobals) {
  // placesOverlay.xul is also included in the browser scope, so include
  // those globals here.
  let globalObjects = Object.assign({}, placesGlobals);
  for (let global of fileGlobals) {
    globalObjects[global.name] = global.writable;
  }
  return globalObjects;
}

function getMozillaCentralItems() {
  return {
    globals: mapGlobals(getScriptGlobals()),
    browserjsScripts: getGlobalScriptsIncludes().concat(EXTRA_SCRIPTS)
  };
}

module.exports = helpers.isMozillaCentralBased() ?
 getMozillaCentralItems() :
 helpers.getSavedEnvironmentItems("browser-window");
