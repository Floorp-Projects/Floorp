/**
 * @fileoverview Reject common XPCOM methods called with useless optional
 *               parameters, or non-existent parameters.
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
  function getRangeAfterArgToEnd(argNumber, args) {
    let sourceCode = context.getSourceCode();
    return [sourceCode.getTokenAfter(args[argNumber]).range[0],
      args[args.length - 1].range[1]];
  }

  // ---------------------------------------------------------------------------
  // Public
  //  --------------------------------------------------------------------------

  return {
    "CallExpression": function(node) {
      let callee = node.callee;
      if (callee.type !== "MemberExpression" ||
          callee.property.type !== "Identifier") {
        return;
      }

      let isFalse = arg => arg.type === "Literal" && arg.value === false;
      let isFalsy = arg => arg.type === "Literal" && !arg.value;
      let isBool = arg => arg.type === "Literal" && (arg.value === false ||
                                                     arg.value === true);
      let name = callee.property.name;
      let args = node.arguments;

      if (["addEventListener", "removeEventListener", "addObserver"]
          .includes(name) && args.length === 3 && isFalse(args[2])) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(1, args));
          },
          message: `${name}'s third parameter can be omitted when it's false.`
        });
      }

      if (name === "clearUserPref" && args.length > 1) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(0, args));
          },
          message: `${name} takes only 1 parameter.`
        });
      }

      if (name === "removeObserver" && args.length === 3 && isBool(args[2])) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(1, args));
          },
          message: "removeObserver only takes 2 parameters."
        });
      }

      if (name === "appendElement" && args.length === 2 && isFalse(args[1])) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(0, args));
          },
          message: `${name}'s second parameter can be omitted when it's false.`
        });
      }

      if (name === "notifyObservers" && args.length === 3 &&
          isFalsy(args[2])) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(1, args));
          },
          message: `${name}'s third parameter can be omitted.`
        });
      }

      if (name === "getComputedStyle" && args.length === 2 &&
          isFalsy(args[1])) {
        context.report({
          node,
          fix: fixer => {
            return fixer.removeRange(getRangeAfterArgToEnd(0, args));
          },
          message: "getComputedStyle's second parameter can be omitted."
        });
      }

      if (name === "newURI" && args.length > 1 &&
          isFalsy(args[args.length - 1])) {
        context.report({
          node,
          fix: fixer => {
            if (args.length > 2 && isFalsy(args[args.length - 2])) {
              return fixer.removeRange(getRangeAfterArgToEnd(0, args));
            }

            return fixer.removeRange(getRangeAfterArgToEnd(args.length - 2, args));
          },
          message: "newURI's last parameters are optional."
        });
      }
    }
  };
};
