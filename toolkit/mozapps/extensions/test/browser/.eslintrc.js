"use strict";

module.exports = {
  "extends": [
    "plugin:mozilla/browser-test"
  ],

  "env": {
    "webextensions": true,
  },

  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^end_test$"}],
  }
};
