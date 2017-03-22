"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": [
    "plugin:mozilla/browser-test"
  ],

  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS|end_test)$"}],
  }
};
