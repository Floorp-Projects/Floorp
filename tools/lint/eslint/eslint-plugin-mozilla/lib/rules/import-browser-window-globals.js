/**
 * @fileoverview For scripts included in browser-window, this will automatically
 *               inject the browser-window global scopes into the file.
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
var browserWindowEnv = require("../environments/browser-window");

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    Program: function(node) {
      let filePath = helpers.getAbsoluteFilePath(context);
      let relativePath = path.relative(helpers.getRootDir(filePath), filePath);

      if (browserWindowEnv.browserjsScripts &&
          browserWindowEnv.browserjsScripts.includes(relativePath)) {
        for (let global in browserWindowEnv.globals) {
          helpers.addVarToScope(global, context.getScope(),
                                browserWindowEnv.globals[global]);
        }
      }
    }
  };
};
