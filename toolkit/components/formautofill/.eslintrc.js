/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  rules: {
    // Rules from the mozilla plugin
    "mozilla/balanced-listeners": "error",
    "mozilla/no-aArgs": "error",
    "mozilla/var-only-at-top-level": "error",

    "valid-jsdoc": [
      "error",
      {
        prefer: {
          return: "returns",
        },
        preferType: {
          Boolean: "boolean",
          Number: "number",
          String: "string",
          bool: "boolean",
        },
        requireParamDescription: false,
        requireReturn: false,
        requireReturnDescription: false,
      },
    ],

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // No declaring variables that are never used
    "no-unused-vars": [
      "error",
      {
        args: "none",
        vars: "all",
      },
    ],

    // No using variables before defined
    "no-use-before-define": "error",

    // Disallow using variables outside the blocks they are defined (especially
    // since only let and const are used, see "no-var").
    "block-scoped-var": "error",

    // Warn about cyclomatic complexity in functions.
    complexity: ["error", { max: 26 }],

    // Maximum depth callbacks can be nested.
    "max-nested-callbacks": ["error", 4],

    // Disallow using the console API.
    "no-console": "error",

    // Disallow fallthrough of case statements, except if there is a comment.
    "no-fallthrough": "error",

    // Disallow use of multiline strings (use template strings instead).
    "no-multi-str": "error",

    // Disallow usage of __proto__ property.
    "no-proto": "error",

    // Disallow use of assignment in return statement. It is preferable for a
    // single line of code to have only one easily predictable effect.
    "no-return-assign": "error",

    // Require use of the second argument for parseInt().
    radix: "error",

    // Require "use strict" to be defined globally in the script.
    strict: ["error", "global"],

    // Disallow Yoda conditions (where literal value comes first).
    yoda: "error",

    // Disallow function or variable declarations in nested blocks
    "no-inner-declarations": "error",
  },
};
