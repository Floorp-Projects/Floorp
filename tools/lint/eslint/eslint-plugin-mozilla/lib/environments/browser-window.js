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

var path = require("path");
var helpers = require("../helpers");
var globals = require("../globals");

const SCRIPTS = [
  //"browser/base/content/nsContextMenu.js",
  "toolkit/content/contentAreaUtils.js",
  "browser/components/places/content/editBookmarkOverlay.js",
  "toolkit/components/printing/content/printUtils.js",
  "toolkit/content/viewZoomOverlay.js",
  "browser/components/places/content/browserPlacesViews.js",
  "browser/base/content/browser.js",
  "browser/components/downloads/content/downloads.js",
  "browser/components/downloads/content/indicator.js",
  "browser/components/customizableui/content/panelUI.js",
  "toolkit/components/viewsource/content/viewSourceUtils.js",
  "browser/base/content/browser-addons.js",
  "browser/base/content/browser-ctrlTab.js",
  "browser/base/content/browser-customization.js",
  "browser/base/content/browser-feeds.js",
  "browser/base/content/browser-fullScreenAndPointerLock.js",
  "browser/base/content/browser-fullZoom.js",
  "browser/base/content/browser-gestureSupport.js",
  "browser/base/content/browser-media.js",
  "browser/base/content/browser-places.js",
  "browser/base/content/browser-plugins.js",
  "browser/base/content/browser-refreshblocker.js",
  "browser/base/content/browser-safebrowsing.js",
  "browser/base/content/browser-sidebar.js",
  "browser/base/content/browser-social.js",
  "browser/base/content/browser-syncui.js",
  "browser/base/content/browser-tabsintitlebar.js",
  "browser/base/content/browser-thumbnails.js",
  "browser/base/content/browser-trackingprotection.js",
  "browser/base/content/browser-data-submission-info-bar.js",
  "browser/base/content/browser-fxaccounts.js",
  // This gets loaded into the same scopes as browser.js via browser.xul and
  // placesOverlay.xul.
  "toolkit/content/globalOverlay.js",
  // Via editMenuOverlay.xul
  "toolkit/content/editMenuOverlay.js"
];

function getScriptGlobals() {
  let fileGlobals = [];
  let root = helpers.getRootDir(module.filename);
  for (let script of SCRIPTS) {
    let fileName = path.join(root, script);
    try {
      fileGlobals = fileGlobals.concat(globals.getGlobalsForFile(fileName));
    } catch (e) {
      throw new Error(`Could not load globals from file ${fileName}: ${e}`)
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
