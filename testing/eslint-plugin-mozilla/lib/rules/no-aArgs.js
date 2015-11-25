/**
 * @fileoverview warns against using hungarian notation in function arguments
 * (i.e. aArg).
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// -----------------------------------------------------------------------------
// Rule Definition
// -----------------------------------------------------------------------------

module.exports = function(context) {
  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  function isPrefixed(name) {
    return name.length >= 2 && /^a[A-Z]/.test(name);
  }

  function deHungarianize(name) {
    return name.substring(1, 2).toLowerCase() +
           name.substring(2, name.length);
  }

  function checkFunction(node) {
    for (var i = 0; i < node.params.length; i++) {
      var param = node.params[i];
      if (param.name && isPrefixed(param.name)) {
        var errorObj = {
          name: param.name,
          suggestion: deHungarianize(param.name)
        };
        context.report(param,
                       "Parameter '{{name}}' uses Hungarian Notation, " +
                       "consider using '{{suggestion}}' instead.",
                       errorObj);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Public
  // ---------------------------------------------------------------------------

  return {
    "FunctionDeclaration": checkFunction,
    "ArrowFunctionExpression": checkFunction,
    "FunctionExpression": checkFunction
  };
};
