"use strict";

module.exports = {
  "rules": {
    // Enforce return statements in callbacks of array methods.
    "array-callback-return": "error",

    // Verify calls of super() in constructors.
    "constructor-super": "error",

    // Require default case in switch statements.
    "default-case": "error",

    // Disallow use of alert(), confirm(), and prompt().
    "no-alert": "error",

    // Disallow likely erroneous `switch` scoped lexical declarations in
    // case/default clauses.
    "no-case-declarations": "error",

    // Disallow use of the console API.
    "no-console": "error",

    // Disallow constant expressions in conditions (except for loops).
    "no-constant-condition": ["error", { "checkLoops": false }],

    // Disallow extending of native objects.
    "no-extend-native": "error",

    // Disallow case statement fallthrough without explicit `// falls through`
    // annotation.
    "no-fallthrough": "error",

    // No reassigning native JS objects or read only globals.
    "no-global-assign": "error",

    // Disallow use of assignment in return statement.
    "no-return-assign": ["error", "always"],

    // Disallow template literal placeholder syntax in regular strings.
    "no-template-curly-in-string": "error",

    // Disallow use of this/super before calling super() in constructors.
    "no-this-before-super": "error",

    // Disallow unmodified loop conditions.
    "no-unmodified-loop-condition": "error",

    // No expressions where a statement is expected
    "no-unused-expressions": "error",

    // Disallow unnecessary escape usage in strings and regular expressions.
    "no-useless-escape": "error",

    // Require "use strict" to be defined globally in the script.
    "strict": ["error", "global"],

    // Enforce valid JSDoc comments.
    "valid-jsdoc": ["error", {
      "requireParamDescription": false,
      "requireReturn": false,
      "requireReturnDescription": false,
    }],

    // Disallow Yoda conditions.
    "yoda": ["error", "never"],
  }
};
