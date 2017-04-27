"use strict";

module.exports = {
  "extends": "../../.eslintrc.js",
  "rules": {
    // Require spacing around =>
    "arrow-spacing": "error",

    // No newline before open brace for a block
    "brace-style": ["error", "1tbs", {"allowSingleLine": true}],

    // No space before always a space after a comma
    "comma-spacing": ["error", {"before": false, "after": true}],

    // Commas at the end of the line not the start
    "comma-style": "error",

    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 43],

    // Use [] instead of Array()
    "no-array-constructor": "error",

    // Use {} instead of new Object()
    "no-new-object": "error",

    // No using undeclared variables
    "no-undef": "error",

    // Don't allow unused local variables unless they match the pattern
    "no-unused-vars": ["error", {"args": "none", "vars": "local", "varsIgnorePattern": "^(ids|ignored|unused)$"}],

    // Always require semicolon at end of statement
    "semi": ["error", "always"],

    // Require spaces around operators
    "space-infix-ops": "error",
  }
};
