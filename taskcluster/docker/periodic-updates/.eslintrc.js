/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    // JS files in this folder are commonly xpcshell scripts where |arguments|
    // is defined in the global scope.
    arguments: false,
  },
  rules: {
    // Enforce return statements in callbacks of array methods.
    "array-callback-return": "error",

    // Require default case in switch statements.
    "default-case": "error",

    // Disallow use of alert(), confirm(), and prompt().
    "no-alert": "error",

    // Disallow likely erroneous `switch` scoped lexical declarations in
    // case/default clauses.
    "no-case-declarations": "error",

    // Disallow constant expressions in conditions (except for loops).
    "no-constant-condition": ["error", { checkLoops: false }],

    // Disallow extending of native objects.
    "no-extend-native": "error",

    // Disallow use of assignment in return statement.
    "no-return-assign": ["error", "always"],

    // Disallow template literal placeholder syntax in regular strings.
    "no-template-curly-in-string": "error",

    // Disallow unmodified loop conditions.
    "no-unmodified-loop-condition": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // Require "use strict" to be defined globally in the script.
    strict: ["error", "global"],

    // Disallow Yoda conditions.
    yoda: ["error", "never"],
  },
};
