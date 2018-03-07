"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/xpcshell-test"
  ],
  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^end_test$"}],
  }
};
