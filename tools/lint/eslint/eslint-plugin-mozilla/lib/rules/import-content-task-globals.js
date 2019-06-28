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

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    "CallExpression[callee.object.name='ContentTask'][callee.property.name='spawn']": function(node) {
      for (let global in frameScriptEnv.globals) {
        helpers.addVarToScope(global, context.getScope(),
                              frameScriptEnv.globals[global]);
      }
    },
  };
};
