"use strict";

module.exports = {
  "env": {
    "webextensions": true,
  },

  "rules": {
    "no-unused-vars": ["error", {"args": "none", "varsIgnorePattern": "^end_test$"}],
  }
};
