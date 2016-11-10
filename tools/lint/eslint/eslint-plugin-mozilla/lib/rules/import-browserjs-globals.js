/**
 * @fileoverview Import globals from browser.js.
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
  "browser/base/content/browser-devedition.js",
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
  "browser/base/content/browser-fxaccounts.js"
];

module.exports = function(context) {
  return {
    Program: function(node) {
      if (helpers.getTestType(this) != "browser" &&
          !helpers.getIsHeadFile(this)) {
        return;
      }

      let filepath = helpers.getAbsoluteFilePath(context);
      let root = helpers.getRootDir(filepath);
      for (let script of SCRIPTS) {
        let fileName = path.join(root, script);
        try {
          let newGlobals = globals.getGlobalsForFile(fileName);
          helpers.addGlobals(newGlobals, context.getScope());
        } catch (e) {
          context.report(
            node,
            "Could not load globals from file {{filePath}}: {{error}}",
            {
              filePath: path.relative(root, fileName),
              error: e
            }
          );
        }
      }
    }
  };
};
