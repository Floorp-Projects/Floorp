"use strict";

module.exports = {
  "extends": "plugin:mozilla/mochitest-test",

  "env": {
    "browser": true,
    "webextensions": true,
  },

  "rules": {
    "no-shadow": 0,
  },
};
