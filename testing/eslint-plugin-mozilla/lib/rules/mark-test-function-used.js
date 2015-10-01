/**
 * @fileoverview Simply marks test (the test method) as used. This avoids ESLint
 * telling us that the function is never called..
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
  //--------------------------------------------------------------------------
  // Public
  //--------------------------------------------------------------------------

  return {
    Program: function(node) {
      context.markVariableAsUsed("test");
    }
  };
};
