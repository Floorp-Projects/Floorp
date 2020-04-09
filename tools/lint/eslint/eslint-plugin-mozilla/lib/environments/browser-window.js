/**
 * @fileoverview Defines the environment when in the browser.xhtml window.
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
var helpers = require("../helpers");
var { getScriptGlobals } = require("./utils");

// When updating EXTRA_SCRIPTS or MAPPINGS, be sure to also update the
// 'support-files' config in `tools/lint/eslint.yml`.

// These are scripts not loaded from browser.xhtml or global-scripts.inc
// but via other includes.
const EXTRA_SCRIPTS = [
  "browser/base/content/nsContextMenu.js",
  "browser/components/places/content/editBookmark.js",
  "browser/components/downloads/content/downloads.js",
  "browser/components/downloads/content/indicator.js",
  "toolkit/content/customElements.js",
  "toolkit/content/editMenuOverlay.js",
];

const extraDefinitions = [
  // Via Components.utils, defineModuleGetter, defineLazyModuleGetters or
  // defineLazyScriptGetter (and map to
  // single) variable.
  { name: "XPCOMUtils", writable: false },
  { name: "Task", writable: false },
  { name: "windowGlobalChild", writable: false },
];

// Some files in global-scripts.inc need mapping to specific locations.
const MAPPINGS = {
  "printUtils.js": "toolkit/components/printing/content/printUtils.js",
  "panelUI.js": "browser/components/customizableui/content/panelUI.js",
  "viewSourceUtils.js":
    "toolkit/components/viewsource/content/viewSourceUtils.js",
  "places-tree.js": "browser/components/places/content/places-tree.js",
  "places-menupopup.js":
    "browser/components/places/content/places-menupopup.js",
};

const globalScriptsRegExp = /^\s*Services.scriptloader.loadSubScript\(\"(.*?)\", this\);$/;

function getGlobalScriptIncludes(scriptPath) {
  let fileData;
  try {
    fileData = fs.readFileSync(scriptPath, { encoding: "utf8" });
  } catch (ex) {
    // The file isn't present, so this isn't an m-c repository.
    return null;
  }

  fileData = fileData.split("\n");

  let result = [];

  for (let line of fileData) {
    let match = line.match(globalScriptsRegExp);
    if (match) {
      let sourceFile = match[1]
        .replace(
          "chrome://browser/content/search/",
          "browser/components/search/content/"
        )
        .replace("chrome://browser/content/", "browser/base/content/")
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

function getGlobalScripts() {
  let results = [];
  for (let scriptPath of helpers.globalScriptPaths) {
    results = results.concat(getGlobalScriptIncludes(scriptPath));
  }
  return results;
}

module.exports = getScriptGlobals(
  "browser-window",
  getGlobalScripts().concat(EXTRA_SCRIPTS),
  extraDefinitions,
  {
    browserjsScripts: getGlobalScripts().concat(EXTRA_SCRIPTS),
  }
);
