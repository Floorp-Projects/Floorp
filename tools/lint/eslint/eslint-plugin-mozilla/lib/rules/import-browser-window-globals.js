/**
 * @fileoverview For scripts included in browser-window, this will automatically
 *               inject the browser-window global scopes into the file.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var path = require("path");
var helpers = require("../helpers");
var browserWindowEnv = require("../environments/browser-window");

module.exports = {
  // This rule currently has no messages.
  // eslint-disable-next-line eslint-plugin/prefer-message-ids
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/import-browser-window-globals.html",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      Program(node) {
        let filePath = helpers.getAbsoluteFilePath(context);
        let relativePath = path.relative(helpers.rootDir, filePath);
        // We need to translate the path on Windows, due to the change
        // from \ to /, and browserjsScripts assumes Posix.
        if (path.win32) {
          relativePath = relativePath.split(path.sep).join("/");
        }

        if (browserWindowEnv.browserjsScripts?.includes(relativePath)) {
          for (let global in browserWindowEnv.globals) {
            helpers.addVarToScope(
              global,
              context.getScope(),
              browserWindowEnv.globals[global]
            );
          }
        }
      },
    };
  },
};
