/**
 * @fileoverview For ContentTask.spawn, this will automatically declare the
 *               frame script variables in the global scope.
 *               Note: due to the way ESLint works, it appears it is only
 *               easy to declare these variables on a file-global scope, rather
 *               than function global.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");
var frameScriptEnv = require("../environments/frame-script");
var sandboxEnv = require("../environments/special-powers-sandbox");

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/import-content-task-globals.html",
    },
    type: "problem",
  },

  create(context) {
    return {
      "CallExpression[callee.object.name='ContentTask'][callee.property.name='spawn']": function(
        node
      ) {
        // testing/mochitest/BrowserTestUtils/content/content-task.js
        // This script is loaded as a sub script into a frame script.
        for (let [name, value] of Object.entries(frameScriptEnv.globals)) {
          helpers.addVarToScope(name, context.getScope(), value);
        }
      },
      "CallExpression[callee.object.name='SpecialPowers'][callee.property.name='spawn']": function(
        node
      ) {
        for (let [name, value] of Object.entries(sandboxEnv.globals)) {
          helpers.addVarToScope(name, context.getScope(), value);
        }
        let globals = [
          // testing/specialpowers/content/SpecialPowersChild.jsm
          // SpecialPowersChild._spawnTask
          "SpecialPowers",
          "ContentTaskUtils",
          "content",
          "docShell",
        ];
        for (let global of globals) {
          helpers.addVarToScope(global, context.getScope(), false);
        }
      },
      "CallExpression[callee.object.name='SpecialPowers'][callee.property.name='spawnChrome']": function(
        node
      ) {
        for (let [name, value] of Object.entries(sandboxEnv.globals)) {
          helpers.addVarToScope(name, context.getScope(), value);
        }
        let globals = [
          // testing/specialpowers/content/SpecialPowersParent.jsm
          // SpecialPowersParent._spawnChrome
          "windowGlobalParent",
          "browsingContext",
        ];
        for (let global of globals) {
          helpers.addVarToScope(global, context.getScope(), false);
        }
      },
    };
  },
};
