"use strict";

module.exports = {
  "rules": {
    // Warn about cyclomatic complexity in functions.
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", {"max": 60}],

    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}],
  }
};
