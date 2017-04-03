"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/browser-test"
  ],

  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS|end_test)$"}],
  }
};
