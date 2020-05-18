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

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

var helpers = require("../helpers");
var frameScriptEnv = require("../environments/frame-script");

// The global environment of SpecialPowers.spawn tasks is
// controlled by the Sandbox environment created by
// SpecialPowersSandbox.jsm. This list should be kept in sync with
// that module.
var sandboxGlobals = [
  "Assert",
  "Blob",
  "BrowsingContext",
  "ChromeUtils",
  "ContentTaskUtils",
  "EventUtils",
  "Services",
  "TextDecoder",
  "TextEncoder",
  "URL",
  "assert",
  "info",
  "is",
  "isnot",
  "ok",
  "todo",
  "todo_is",
];

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    "CallExpression[callee.object.name='ContentTask'][callee.property.name='spawn']": function(
      node
    ) {
      for (let global in frameScriptEnv.globals) {
        helpers.addVarToScope(
          global,
          context.getScope(),
          frameScriptEnv.globals[global]
        );
      }
    },
    "CallExpression[callee.object.name='SpecialPowers'][callee.property.name='spawn']": function(
      node
    ) {
      let globals = [...sandboxGlobals, "SpecialPowers", "content", "docShell"];
      for (let global of globals) {
        helpers.addVarToScope(global, context.getScope(), false);
      }
    },
    "CallExpression[callee.object.name='SpecialPowers'][callee.property.name='spawnChrome']": function(
      node
    ) {
      let globals = [
        ...sandboxGlobals,
        "browsingContext",
        "windowGlobalParent",
      ];
      for (let global of globals) {
        helpers.addVarToScope(global, context.getScope(), false);
      }
    },
  };
};
