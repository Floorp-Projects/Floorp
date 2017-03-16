"use strict";

module.exports = { // eslint-disable-line no-undef
  "rules": {
    // Warn about cyclomatic complexity in functions.
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the toolkit/.eslintrc.js value.
    "complexity": ["error", {"max": 60}],

    // No using undeclared variables
    "no-undef": "error",

    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}],
  }
};
