"use strict";

module.exports = { // eslint-disable-line no-undef
  "rules": {
    // No using undeclared variables
    "no-undef": "error",

    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS)$"}],
  }
};
