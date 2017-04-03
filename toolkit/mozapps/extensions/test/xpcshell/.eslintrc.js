"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],
  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^(Cc|Ci|Cr|Cu|EXPORTED_SYMBOLS|end_test)$"}],
  }
};
